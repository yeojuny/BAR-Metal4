"""Add the bounded Sepia functional fixture to a fresh private game profile.

This edits only the supplied private profile. It never edits an engine source,
installed data tree, app, or release wrapper.
"""

import argparse
import hashlib
import json
import os
import re
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED

HERE = Path(__file__).resolve().parent
CONTROL = HERE.parent/'common/control.lua'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def replace_once(text, old, new):
    assert text.count(old) == 1, (old, text.count(old))
    return text.replace(old, new, 1)


def controller_text():
    original = CONTROL.read_text()
    text = replace_once(original,
        "assert(warm and Spring.DiffTimers(now,warm)>=9.8,'[MatchedGameplay] FAILED_WARMUP_BEFORE_SIM300')",
        "assert(warm,'[MatchedGameplay] FAILED_NO_POSITIVE_SIMULATION')\n"
        " Spring.Echo(string.format('[V2FunctionalWarmup] observed_seconds=%.9f gate=observational',Spring.DiffTimers(now,warm)))")
    text = replace_once(text,
        'if f==1500 then endMeasurement(f) end',
        "if f==1500 then endMeasurement(f);Spring.SendCommands('pause 1') end")
    text = replace_once(text,
        'if frameStopped and game>=1500 then',
        "if frameStopped and game>=1500 then\n  if not WG.v2FunctionalDone then\n   if Spring.DiffTimers(now,opened)>=180 then Spring.Echo('[V2Functional] FAILED_CONTROLLER_TIMEOUT');Spring.Quit() end\n   return\n  end")
    return text


def generated_overlay(source):
    # The generated GLSL strings and uniformInt table remain byte-for-byte
    # intact. Only the returned Lua table gets private runtime values.
    assert source.count('return {vertex=') == 1
    assert re.search(r'uniform float exposure\s*=\s*1(?:\.0)?\s*;', source)
    assert re.search(r'uniform vec3 bias\s*=\s*vec3\(0(?:\.0)?\)\s*;', source)
    text = source.replace('return {vertex=', 'local generated = {vertex=', 1).rstrip()
    text += '''

local exposure = Spring.GetConfigFloat('V2PilotExposure',1.0)
local red = Spring.GetConfigFloat('V2PilotBiasR',0.0)
local green = Spring.GetConfigFloat('V2PilotBiasG',0.0)
local blue = Spring.GetConfigFloat('V2PilotBiasB',0.0)
generated.uniforms = {
 exposure={values={exposure},integer=false},
 bias={values={red,green,blue},integer=false},
}
Spring.Echo(string.format('[V2GeneratedUniforms] exposure=%.3f bias=%.3f,%.3f,%.3f source=private_generated_module',
 exposure,red,green,blue))
return generated
'''
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('old', 'shared'), required=True)
    parser.add_argument('--data', type=Path, required=True, help='fresh private state/data directory')
    parser.add_argument('--generated-base', type=Path, required=True, help='signed candidate generated Lua module')
    args = parser.parse_args()
    data = args.data.resolve()
    assert data.is_dir() and (data/'LuaUI/Config/BC213.lua').is_file()
    assert (data/'LuaUI/Widgets/zzz_native_short_run.lua').is_file()
    assert not (data/'infolog.txt').exists(), 'Fixture must be installed before first game'
    base = args.generated_base.resolve()
    assert base.is_file()

    controller = data/'LuaUI/Widgets/zzz_native_short_run.lua'
    assert sha(controller) == sha(CONTROL), 'Expected unchanged frozen controller before fixture patch'
    controller.write_text(controller_text())
    probe = data/'LuaUI/Widgets/zzz_v2_functional_probe.lua'
    assert not probe.exists(), 'Raw probe must remain absent; handler requires archive origin'
    probe_source = HERE/'v2_recovery_widget.lua'
    probe_bytes = probe_source.read_bytes()
    assert b'handler=true' in probe_bytes
    package = data/'games/bar-commander-control-213c1.sdz'
    with ZipFile(package) as source_zip:
        existing = [(entry,source_zip.read(entry.filename)) for entry in source_zip.infolist()]
    assert [entry.filename for entry,_ in existing] == [
        'modinfo.lua','LuaRules/Gadgets/bench_commander_213c1.lua']
    original_hashes = {entry.filename:hashlib.sha256(payload).hexdigest() for entry,payload in existing}
    tmp = package.with_name(package.name+'.v2-new')
    assert not tmp.exists()
    with ZipFile(tmp,'w') as target_zip:
        for entry,payload in existing:target_zip.writestr(entry,payload)
        added = ZipInfo('LuaUI/Widgets/zzz_v2_functional_probe.lua')
        added.compress_type = ZIP_DEFLATED
        target_zip.writestr(added,probe_bytes)
    os.replace(tmp,package)
    with ZipFile(package) as target_zip:
        final_names = target_zip.namelist()
        assert final_names == list(original_hashes)+['LuaUI/Widgets/zzz_v2_functional_probe.lua']
        assert all(hashlib.sha256(target_zip.read(name)).hexdigest()==digest
                   for name,digest in original_hashes.items())
        assert target_zip.read(final_names[-1])==probe_bytes

    cfg = data/'LuaUI/Config/BC213.lua'
    prior = cfg.read_text()
    cfg.write_text(replace_once(prior,
        'order = { ["Limit idle FPS"] = 0, ["Native Precount Warmup Pacing"] = 1 }',
        'order = { ["Limit idle FPS"] = 0, ["Native Precount Warmup Pacing"] = 1, '
        '["Sepia Tone"] = 1, ["V2 Sepia Functional Probe"] = 1 }'))
    settings = data/'springsettings.cfg'
    current = settings.read_text()
    assert not re.search(r'^V2PilotMode\s*=', current, re.M)
    settings.write_text(current + '\nV2PilotMode = ' + args.mode + '\n')

    generated = data/'LuaUI/NativeWidgets/sepia_pilot_program.lua'
    if args.mode == 'shared':
        generated.parent.mkdir(parents=True, exist_ok=True)
        assert not generated.exists(), 'Private generated module already installed'
        generated.write_text(generated_overlay(base.read_text()))
    else:
        assert not generated.exists(), 'Old comparator must read archive original only'

    manifest = {
        'mode': args.mode,
        'privateData': str(data),
        'frozenControllerSHA256': sha(CONTROL),
        'stagedControllerSHA256': sha(controller),
        'originalWidgetName': 'Sepia Tone',
        'originalWidgetShaderControl': 'WG.sepia.setSepia(0.5); shadeUI false',
        'moduleSelection': 'VFS.RAW_FIRST from private profile only in shared mode',
        'generatedBaseSHA256': sha(base),
        'privateGeneratedModuleSHA256': sha(generated) if generated.exists() else None,
        'privateFunctionalProbeSHA256': sha(probe_source),
        'privateFunctionalProbeOrigin': 'ZIP_ONLY archive member with GetInfo.handler=true; no raw duplicate',
        'originalArchiveMemberSHA256': original_hashes,
        'privateGameOverlaySHA256': sha(package),
        'privateWidgetConfigSHA256': sha(cfg),
        'privateSettingsSHA256': sha(settings),
        'images': ['v2-old-identity.png'] if args.mode == 'old' else
                  ['v2-shared-identity.png','v2-shared-variant.png'],
        'uniformControl': 'Spring config overlay only; generated GLSL strings and native source unmodified',
    }
    (data/'LuaUI/Config/v2-functional-fixture.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2))


if __name__ == '__main__':
    main()

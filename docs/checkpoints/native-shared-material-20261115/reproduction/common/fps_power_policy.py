"""Disable BAR's offscreen/idle FPS widget in writable preferences only.

Call apply(data, short_name='BC213') after the original fresh-profile seed check,
before recording its launch checksums. Do not apply to historical run profiles.
"""
from pathlib import Path
import hashlib
import re


def digest(text):
    return hashlib.sha256(text.encode()).hexdigest()


def apply(data, short_name='BYAR'):
    data = Path(data)
    cfg = data / 'springsettings.cfg'
    ui = data / 'LuaUI/Config' / (short_name + '.lua')
    assert cfg.is_file(), cfg
    changes = []
    for path in (cfg, ui):
        existed = path.exists()
        before = path.read_text() if existed else ''
        after = before
        keys = []
        if path == cfg:
            pattern = r'(?m)^(\s*LimitIdleFps\s*=\s*)[^\r\n]*'
            if re.search(pattern, before):
                after, count = re.subn(pattern, r'\g<1>0', after)
                assert count == 1
            else:
                after += ('' if not after or after.endswith('\n') else '\n') + 'LimitIdleFps = 0\n'
            keys.append('LimitIdleFps=0')
        elif existed:
            pattern = r'(\["Limit idle FPS"\]\s*=\s*)\d+'
            after, count = re.subn(pattern, r'\g<1>0', after)
            assert count == 1, f'Expected one widget order entry: {path}'
            for key in ('limitoffscreenfps', 'limitidlefps'):
                after = re.sub(r'(\b' + key + r'\s*=\s*)(?:true|false)', r'\g<1>false', after)
            keys += ['order["Limit idle FPS"]=0', 'cached power option values=false when present']
        else:
            # This macro previously had no config: retain default settings for
            # every other widget, without inheriting an unrelated BYAR profile.
            after = '-- Widget order: 0 disables the offscreen and idle FPS widget.\nreturn { allowUserWidgets = true, data = {}, order = { ["Limit idle FPS"] = 0 } }\n'
            keys.append('order["Limit idle FPS"]=0; all other widget settings remain default')
        if after != before:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(after)
        assert path.read_text() == after
        changes.append(dict(path=str(path), existedBefore=existed, changed=before != after,
                            beforeSHA256=digest(before) if existed else None,
                            afterSHA256=digest(after), policy=keys))
    return changes


def install(harness, short_name='BC213'):
    """Adapt a future runner; its baseline seed checks still run first."""
    original = harness.stage_arm

    def stage_arm(*args, **kwargs):
        prepared = original(*args, **kwargs)
        data, output, env, plan = prepared
        changes = apply(data, short_name)
        for change in changes:
            plan['checksums'][change['path']] = change['afterSHA256']
        plan['fpsPowerPolicy'] = changes
        return prepared

    harness.stage_arm = stage_arm

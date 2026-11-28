"""Reuse the exercised power/display checks with this task's timed awake owner."""
from pathlib import Path
import datetime
import json
import os
import re
import subprocess

R = Path(__file__).resolve().parent
C = Path('/Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance')


def preflight(remaining):
    awake = json.loads((R / 'AWAKE.json').read_text())
    assert awake['status'] == 'ACTIVE' and not awake['securitySettingsChanged']
    os.kill(awake['pid'], 0)
    assert '/usr/bin/caffeinate' in subprocess.check_output(
        ['/bin/ps', '-p', str(awake['pid']), '-o', 'args='], text=True)
    age = (datetime.datetime.now(datetime.timezone.utc) -
           datetime.datetime.fromisoformat(awake['startUTC'])).total_seconds()
    assert age + 330 * remaining < awake['maximumSeconds']
    display = json.loads(subprocess.check_output(
        ['/usr/bin/python3', '-B', str(C / 'watch_display110.py'), '--once'], text=True))
    assert display['online'] and display['active'] and not display['asleep'], display
    battery = subprocess.check_output(['/usr/bin/pmset', '-g', 'batt'], text=True)
    settings = subprocess.check_output(['/usr/bin/pmset', '-g', 'custom'], text=True).partition('AC Power:')[2]
    assert "'AC Power'" in battery.splitlines()[0] and re.search(r'^\s*powermode\s+0\s*$', settings, re.M)
    thermal = subprocess.check_output(['/usr/bin/pmset', '-g', 'therm'], text=True)
    assert not re.search(r'(?:thermal|performance) warning level\s*[:=]\s*[1-9]|(?:Thermal_Level|Performance_Level)\s*=\s*[1-9]', thermal, re.I)
    return dict(display=display, battery=battery, thermal=thermal,
                awakeOwner=str(R / 'AWAKE.json'),
                offscreenFrameReduction='disabled by reused217 profile',
                lockPolicy='User permits locked observation only if actual GPU records still complete')

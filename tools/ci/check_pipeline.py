#!/usr/bin/env python3
from pathlib import Path
import json,sys,yaml
root=Path(__file__).resolve().parents[2];errors=[]
try:p=json.loads((root/'CMakePresets.json').read_text())
except Exception as e:errors.append(f'CMakePresets.json: {e}');p={}
names={x.get('name') for x in p.get('configurePresets',[])}
for name in ('linux-gcc-debug','windows-msvc-debug','windows-msvc-release'):
    if name not in names:errors.append(f'missing configure preset {name}')
manifest=(root/'vcpkg.json').read_text()
if 'REPLACE_WITH' in manifest:errors.append('vcpkg manifest contains a placeholder')
for rel in ('.github/workflows/ci.yml','.github/workflows/release.yml'):
    path=root/rel
    try:yaml.safe_load(path.read_text())
    except Exception as e:errors.append(f'{rel}: {e}')
    text=path.read_text()
    for forbidden in ('@main','@master','pull_request_target'):
        if forbidden in text:errors.append(f'{rel}: forbidden {forbidden}')
release=(root/'.github/workflows/release.yml').read_text()
for required in ('tags:','windows-msvc-release-bundle','Get-FileHash','contents: write'):
    if required not in release:errors.append(f'release workflow missing {required}')
pack=(root/'cmake/Packaging.cmake').read_text()
for required in ('squiflow_workstation','windeployqt','squiflow_archive'):
    if required not in pack:errors.append(f'packaging missing {required}')
if errors:print('\n'.join(errors),file=sys.stderr);sys.exit(1)
print('pipeline policy: presets, workflows, packaging and manifest passed')

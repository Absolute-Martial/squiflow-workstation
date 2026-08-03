#!/usr/bin/env python3
from pathlib import Path
import re,sys
root=Path(__file__).resolve().parents[2];text=(root/'src/app/composition_root.cpp').read_text();dirs=sorted(p.name for p in (root/'src/modules').iterdir() if p.is_dir())
includes=sorted(re.findall(r'modules/([^/]+)/module.hpp',text));calls=sorted(re.findall(r'modules::([a-z_]+)::make_module',text));errors=[]
if includes!=dirs:errors.append(f'includes {includes} != module dirs {dirs}')
if calls!=dirs:errors.append(f'factories {calls} != module dirs {dirs}')
if 'r.seal();' not in text:errors.append('registry is not sealed')
if errors:print('\n'.join(errors),file=sys.stderr);sys.exit(1)
print(f'composition root policy: {len(dirs)} modules passed')

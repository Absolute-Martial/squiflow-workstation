#!/usr/bin/env python3
from pathlib import Path
import re,sys
root=Path(__file__).resolve().parents[2];errors=[]
main=(root/'src/app/main.cpp').read_text().splitlines()
if len(main)>20:errors.append(f'main.cpp has {len(main)} lines (limit 20)')
for p in (root/'src/app').glob('*.[ch]pp'):
 t=p.read_text()
 for token in ('TODO','FIXME','.detach(','sleep_for','QTimer','SetTimer','CreateTimerQueueTimer'):
  if token in t:errors.append(f'{p.relative_to(root)}: prohibited {token}')
 if p.name!='composition_root.cpp' and len(set(re.findall(r'modules/([^/]+)/module.hpp',t)))>1:errors.append(f'{p.relative_to(root)}: module fan-in outside composition root')
if errors:print('\n'.join(errors),file=sys.stderr);sys.exit(1)
print('application architecture policy: passed')

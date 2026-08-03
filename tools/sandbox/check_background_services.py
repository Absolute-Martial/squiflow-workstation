#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
files=sorted((root/'src/platform').glob('background_*.[ch]pp'))
prohibited=('QTimer','timerEvent','startTimer','SetTimer','CreateTimerQueueTimer','timeBeginPeriod','sleep_for','sleep_until','.detach(','system(')
errors=[]
for path in files:
 text=path.read_text()
 for token in prohibited:
  if token in text: errors.append(f'{path.relative_to(root)}: prohibited API {token}')
 if path.name not in {'background_executor.hpp','background_executor.cpp'} and ('std::jthread' in text or 'std::thread' in text): errors.append(f'{path.relative_to(root)}: service-level thread ownership')
if errors: print('\n'.join(errors),file=sys.stderr);raise SystemExit(1)
print(f'background service static policy: {len(files)} files passed')

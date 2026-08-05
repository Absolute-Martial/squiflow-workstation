#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2];ui=root/'src/ui'
cmake=(ui/'CMakeLists.txt').read_text()+(root/'src/app'/'CMakeLists.txt').read_text()
errors=[]
for p in ui.rglob('*.qml'):
 rel=p.relative_to(ui).as_posix();text=p.read_text()
 if rel not in cmake:errors.append(f'{rel}: not embedded')
 if 'Qt.quit(' in text:errors.append(f'{rel}: Qt.quit prohibited')
 if 'FluentWinUI3' in text or 'Fusion' in text:errors.append(f'{rel}: screen selects a style')
conf=(ui/'qtquickcontrols2.conf').read_text()
if 'Style=Fusion' not in conf or 'FallbackStyle=Basic' not in conf:errors.append('central style fallback missing')
if errors:print('\n'.join(errors),file=sys.stderr);sys.exit(1)
print('UI resource policy: compiled QML and central style passed')

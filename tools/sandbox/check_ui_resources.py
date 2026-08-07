#!/usr/bin/env python3
from pathlib import Path
import re
import sys
root=Path(__file__).resolve().parents[2];ui=root/'src/ui'
cmake=(ui/'CMakeLists.txt').read_text()+(root/'src/app'/'CMakeLists.txt').read_text()
errors=[]
# The central style is whatever src/ui/qtquickcontrols2.conf declares (see
# docs/plan/phase-7-fluent-ui-sourcing.md, Layer 1). This check does not
# hardcode a style name: it only requires that exactly one central Style=/
# FallbackStyle= declaration exists, and that no individual screen names
# that same style directly -- style selection must stay centralized so a
# future style change (or a Layer 2/3 library swap) is a one-line edit here,
# not a hunt through every screen.
conf=(ui/'qtquickcontrols2.conf').read_text()
style_match=re.search(r'^Style=(\S+)',conf,re.MULTILINE)
fallback_match=re.search(r'^FallbackStyle=(\S+)',conf,re.MULTILINE)
if not style_match or not fallback_match:errors.append('central style fallback missing')
central_style=style_match.group(1) if style_match else None
for p in ui.rglob('*.qml'):
 rel=p.relative_to(ui).as_posix();text=p.read_text()
 if rel not in cmake:errors.append(f'{rel}: not embedded')
 if 'Qt.quit(' in text:errors.append(f'{rel}: Qt.quit prohibited')
 if central_style and re.search(rf'\b{re.escape(central_style)}\b',text):errors.append(f'{rel}: screen selects a style')
if errors:print('\n'.join(errors),file=sys.stderr);sys.exit(1)
print('UI resource policy: compiled QML and central style passed')

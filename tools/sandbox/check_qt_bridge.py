#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2];errors=[]
def require(path,tokens):
 text=(root/path).read_text()
 for token in tokens:
  if token not in text:errors.append(f'{path}: missing {token}')
require(Path('src/shell/paged_list_model_qt.hpp'),['QAbstractListModel','StableIdRole'])
require(Path('src/shell/paged_list_model_qt.cpp'),['Qt::QueuedConnection','QPointer','beginResetModel'])
require(Path('src/shell/navigation_model_qt.hpp'),['QAbstractListModel','StableIdRole','SelectedRole','ModuleIdRole'])
require(Path('src/shell/navigation_model_qt.cpp'),['beginResetModel','"stableId"','"componentUrl"','"selected"'])
require(Path('src/shell/navigation_bridge_qt.cpp'),['Qt::QueuedConnection','QPointer','apply_access','select'])
require(Path('src/shell/qml_surface_qt.cpp'),['setContextProperty("applicationSurface"','loadFromModule','rootObjects().isEmpty()','lifecycle_.request_shutdown()'])
require(Path('src/shell/primitive_mapping.hpp'),['std::int64_t','format_minor_units'])
require(Path('src/ui/CMakeLists.txt'),['qt_bridge_test','AUTOMOC','qml_surface_qt.cpp'])
if errors:print('\n'.join(errors),file=sys.stderr);sys.exit(1)
print('Qt bridge policy: dispatcher, model, lifecycle and Qt test wiring passed')

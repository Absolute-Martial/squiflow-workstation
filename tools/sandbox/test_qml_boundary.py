#!/usr/bin/env python3
from pathlib import Path
import subprocess,sys,tempfile
checker=Path(__file__).with_name('check_qml_boundary.py')
def run(files,expected):
 with tempfile.TemporaryDirectory() as folder:
  root=Path(folder)
  for name,text in files.items():
   path=root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text)
  result=subprocess.run([sys.executable,str(checker),'--root',str(root)],capture_output=True,text=True,check=False)
  if (result.returncode==0)!=expected:raise AssertionError(result.stdout+result.stderr)
run({'src/engine/value.hpp':'#pragma once\nstruct Value {};\n','src/shell/view.hpp':'#pragma once\nclass QString;\n'},True)
run({'src/engine/value.hpp':'#pragma once\n#include <QObject>\n'},False)
run({'src/app/service.cpp':'void f(){ qmlRegisterType<int>(); }\n'},False)
run({'src/ui/Main.qml':'import SquiFlow.Domain 1.0\nItem {}\n'},False)
run({'src/shell/order.hpp':'#pragma once\nQ_PROPERTY(Order* order READ order)\n'},False)
run({'src/shell/title.hpp':'#pragma once\nQ_PROPERTY(QString title READ title)\n'},True)
print('QML boundary policy tests: 6 cases passed')

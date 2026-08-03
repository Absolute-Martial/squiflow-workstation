#!/usr/bin/env python3
"""Enforce the one-way Qt/QML presentation seam."""
from pathlib import Path
import argparse,re,sys
QT_TOKENS=(r'#include\s*[<"]Q[A-Za-z]',r'\bQObject\b',r'\bQ_OBJECT\b',r'\bQ_PROPERTY\b',r'\bQAbstractListModel\b',r'\bQString\b',r'\bQVariant\b',r'\bQUrl\b',r'\bQDateTime\b',r'\bQt::')
QML_DOMAIN_IMPORTS=(r'^\s*import\s+SquiFlow\.(?:Domain|Engine|Modules)\b',)
ENTITY=re.compile(r'\b(?:Quotation|Order|Party|Product|Invoice|Payment|Agreement|Job)\s*\*')
def files(root,base,suffixes):
 p=root/base
 return [] if not p.exists() else [x for x in p.rglob('*') if x.is_file() and x.suffix in suffixes]
def check(root):
 errors=[]
 pure=[]
 for base in ('src/engine','src/modules','src/workflows'):
  pure+=files(root,base,{'.hpp','.cpp'})
 app=files(root,'src/app',{'.hpp','.cpp'})
 pure += [p for p in app if p.name != 'main.cpp']
 for p in pure:
  text=p.read_text(encoding='utf-8')
  for pattern in QT_TOKENS:
   if re.search(pattern,text):
    errors.append(f'{p.relative_to(root)}: Qt token below src/shell boundary')
    break
  if re.search(r'\b(?:qmlRegister\w*|setContextProperty)\s*[<(]',text):
   errors.append(f'{p.relative_to(root)}: QML registration below src/shell boundary')
 for p in files(root,'src/ui',{'.qml'}):
  text=p.read_text(encoding='utf-8')
  for pattern in QML_DOMAIN_IMPORTS:
   if re.search(pattern,text,re.M):errors.append(f'{p.relative_to(root)}: direct domain QML import')
 for p in files(root,'src/shell',{'.hpp','.cpp'}):
  text=p.read_text(encoding='utf-8')
  for line_number,line in enumerate(text.splitlines(),1):
   if 'Q_PROPERTY' in line and (ENTITY.search(line) or 'std::' in line):
    errors.append(f'{p.relative_to(root)}:{line_number}: raw domain/STL Q_PROPERTY')
 return sorted(errors)
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2]);args=parser.parse_args();errors=check(args.root.resolve())
 if errors:print('\n'.join(errors),file=sys.stderr);return 1
 print('QML boundary policy: pure core and adapter seam passed');return 0
if __name__=='__main__':sys.exit(main())

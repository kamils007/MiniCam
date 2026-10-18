"""Bounded reader for numeric gettext PO resources used by the diagnostic host."""
import ast
import hashlib
import json
import os
from pathlib import Path
import re
import time
import warnings

NAMES = {'solidimport.po': 'solidimport', 'aedit.po': 'aedit'}
MAX_BYTES = 8 * 1024 * 1024


def parse_po(raw, stem):
    if len(raw) > MAX_BYTES:
        raise ValueError('Catalog exceeds 8 MiB')
    text=raw.decode('utf-8-sig',errors='strict')
    entries=[];fields={};active=None;fuzzy=False;plural=False

    def finish():
        nonlocal fields,active,fuzzy,plural
        if fields:
            entries.append((fields,fuzzy,plural))
        fields={};active=None;fuzzy=False;plural=False

    def quoted(value):
        if not value.startswith('"') or not value.endswith('"'):
            raise ValueError('Expected a quoted PO string')
        with warnings.catch_warnings():
            warnings.simplefilter('error')
            result=ast.literal_eval(value)
        if type(result) is not str or '\0' in result:
            raise ValueError('Invalid PO string')
        return result

    for line_number,line in enumerate(text.splitlines(),1):
        line=line.strip()
        try:
            if not line:
                finish();continue
            if line.startswith('#'):
                if line.startswith('#,') and 'fuzzy' in [flag.strip() for flag in line[2:].split(',')]:
                    fuzzy=True
                continue
            match=re.fullmatch(r'(msgctxt|msgid|msgstr|msgid_plural|msgstr\[\d+\])\s+(".*")',line)
            if match:
                field,value=match.groups()
                if field=='msgctxt' and 'msgid' in fields or field=='msgid' and 'msgstr' in fields:
                    finish()
                if field in fields:
                    raise ValueError('Duplicate PO field')
                fields[field]=quoted(value);active=field
                plural=plural or field=='msgid_plural' or field.startswith('msgstr[')
            elif line.startswith('"') and active:
                fields[active]+=quoted(line)
            else:
                raise ValueError('Unsupported PO syntax')
        except (SyntaxError,ValueError,Warning) as error:
            raise ValueError('PO line %d: %s' % (line_number,error)) from error
    finish()
    header='';values={};seen=set()
    for fields,fuzzy,plural in entries:
        context=fields.get('msgctxt','')
        key=fields.get('msgid')
        if key=='' and not context:
            header=fields.get('msgstr','');continue
        if context.casefold()!=stem.casefold()+'|' or key is None:
            continue
        if not re.fullmatch(r'\d+-[1-9]\d*',key):
            continue
        if key in seen:
            raise ValueError('Ambiguous duplicate numeric catalog key: '+key)
        seen.add(key)
        value=fields.get('msgstr','')
        if not fuzzy and not plural and value and len(value)<=32768:
            values[key]=value
    language=next((line.split(':',1)[1].strip() for line in header.splitlines()
                   if line.lower().startswith('language:')),'')
    if not values or not language:
        raise ValueError('Missing numeric translations or Language header for '+stem)
    return {'language':language,'context':stem+'|','entries':values}


def find_catalogs(root, timeout=12):
    """Read filenames only, under the selected install tree; skip reparse points."""
    root=Path(root).resolve();found=[];started=time.monotonic();count=0
    for directory,dirs,files in os.walk(root,followlinks=False):
        count+=1
        if count>20000 or time.monotonic()-started>timeout:
            raise TimeoutError('Catalog search limit; provide --catalog-dir for the language folder')
        dirs[:]=sorted(d for d in dirs if not (Path(directory)/d).is_symlink()
                       and not (getattr((Path(directory)/d).lstat(),'st_file_attributes',0)&0x400))
        for name in sorted(files):
            path=Path(directory)/name
            if name.casefold() in NAMES and not path.is_symlink() and not (getattr(path.lstat(),'st_file_attributes',0)&0x400):
                found.append(path)
                if len(found)>128:
                    raise ValueError('Too many matching catalogs; provide --catalog-dir')
    return found


def prepare_catalogs(root, output, language='pl-PL', catalog_dir=None):
    report={'schema':'solidimport-catalog-files-12-v1','requested_language':language,
            'search_root':str(Path(catalog_dir or root).resolve()),'candidates':[], 'selected':{}}
    mapping={'schema':'solidimport-text-map-12-v1','catalogs':{}}
    try:
        for path in find_catalogs(catalog_dir or root):
            item={'path':str(path.resolve()),'catalog':NAMES[path.name.casefold()]}
            report['candidates'].append(item)
            try:
                size=path.stat().st_size
                if size>MAX_BYTES:raise ValueError('Catalog exceeds 8 MiB')
                raw=path.read_bytes();parsed=parse_po(raw,item['catalog'])
                item.update(sha256=hashlib.sha256(raw).hexdigest(),size_bytes=len(raw),
                            language=parsed['language'],entry_count=len(parsed['entries']))
                item['_parsed']=parsed
            except (OSError,ValueError,UnicodeError) as error:
                item['error']=str(error)
        for stem in ('solidimport','aedit'):
            choices=[item for item in report['candidates'] if item['catalog']==stem and '_parsed' in item]
            matching=[item for item in choices if item['language'].casefold()==language.casefold()]
            if matching:choices=matching
            if not choices:
                if stem=='solidimport':raise ValueError('No usable SolidImport.po found in selected directory')
                continue
            if len({item['sha256'] for item in choices})!=1:
                raise ValueError('Ambiguous '+stem+'.po; provide --catalog-dir for one language folder')
            item=choices[0];parsed=item['_parsed']
            mapping['catalogs'][stem]={'path':item['path'],'sha256':item['sha256'],**parsed}
            report['selected'][stem]={key:item[key] for key in ('path','sha256','language','entry_count')}
            report['selected'][stem]['entry_sha256']={key:hashlib.sha256(value.encode('utf-8')).hexdigest()
                                                     for key,value in parsed['entries'].items()}
        if '10-1' not in mapping['catalogs']['solidimport']['entries']:
            raise ValueError('Selected SolidImport.po lacks requested 10-1 entry')
        temp=Path(output)/'temp';temp.mkdir(exist_ok=True)
        (temp/'catalog_map.json').write_text(json.dumps(mapping,ensure_ascii=False),encoding='utf-8')
        return report
    finally:
        for item in report['candidates']:item.pop('_parsed',None)
        (Path(output)/'catalog_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


def sources_unchanged(catalog_report):
    selected=catalog_report.get('selected',{})
    return bool(selected) and all(hashlib.sha256(Path(item['path']).read_bytes()).hexdigest()==item['sha256']
                                  for item in selected.values())

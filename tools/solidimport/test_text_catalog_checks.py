"""Real catalog parsing and rejection of unsupported/mismatched text evidence."""
import copy
import hashlib
from pathlib import Path
import tempfile
import unittest

from text_catalog import parse_po,prepare_catalogs,sources_unchanged
from probe_solidimport_text_catalog import confirmed_catalog_bridge
from test_solidimport_special_key_checks import sample as base_sample


PO=('msgid ""\nmsgstr ""\n"Language: pl-PL\\n"\n\n'
    'msgctxt "solidimport|"\nmsgid "10-1"\nmsgstr "Wiadomość \\"test\\""\n')


def sample():
    r=base_sample();w=r['worker'];digest='a'*64
    r['catalog_sources_unchanged']=True
    r['catalog']={'selected':{'solidimport':{'sha256':'b'*64,'entry_sha256':{'10-1':digest}}}}
    w.update(schema='solidimport-text-catalog-worker-12-v1',
        text_catalog_sources_unchanged=True,text_catalog_map_checked=True,
        text_catalog_served_count=1,post_text_catalog_host_request=True,
        text_catalog_reads=[{'catalog':'solidimport','key':'10-1','requested_file':'SolidImport.po',
            'source_sha256':'b'*64,'value_utf8_sha256':digest,'thread_id':42,'character_count':12}])
    return r


class TextCatalogChecks(unittest.TestCase):
    def test_utf8_bom_context_and_multiline_translation(self):
        raw=('\ufeff'+PO+'"\\nDrugi wiersz"\n').encode('utf-8')
        p=parse_po(raw,'solidimport')
        self.assertEqual(p['language'],'pl-PL')
        self.assertEqual(p['entries']['10-1'],'Wiadomość "test"\nDrugi wiersz')
        self.assertRaises(ValueError,parse_po,raw,'aedit')

    def test_missing_fuzzy_plural_and_duplicates_never_produce_fallbacks(self):
        for raw in [PO.replace('msgctxt', '#, fuzzy\nmsgctxt'),
                    PO.replace('msgstr "Wiadomość \\"test\\""','msgstr ""'),
                    PO+'\n'+PO.split('\n\n')[1],
                    PO.replace('msgstr "Wiadomość \\"test\\""', 'msgid_plural "10-many"\nmsgstr[0] "Plural"')]:
            self.assertRaises(ValueError,parse_po,raw.encode(),'solidimport')
        self.assertRaises(ValueError,parse_po,b'msgstr __import__("os")','solidimport')

    def test_preparation_selects_actual_source_and_detects_changes(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)/'install';root.mkdir();out=Path(t)/'out';out.mkdir()
            source=root/'SolidImport.po';source.write_text(PO,encoding='utf-8')
            report=prepare_catalogs(root,out)
            self.assertTrue(sources_unchanged(report))
            self.assertEqual(report['selected']['solidimport']['entry_sha256']['10-1'],
                             hashlib.sha256('Wiadomość "test"'.encode()).hexdigest())
            self.assertNotIn('Wiadomość',(out/'catalog_report.json').read_text())
            source.write_text(PO+'\n# changed',encoding='utf-8')
            self.assertFalse(sources_unchanged(report))
            second=root/'second';second.mkdir();(second/'SolidImport.po').write_text(PO,encoding='utf-8')
            self.assertRaises(ValueError,prepare_catalogs,root,out)

    def test_accepted_catalog_is_distinct_from_full_init(self):
        r=sample();self.assertTrue(confirmed_catalog_bridge(r))
        self.assertFalse(r['worker']['init_returned'])
        self.assertFalse(r['worker']['init_succeeded'])
        r['worker_exit_code']=0
        r['worker'].update(phase='completed',init_returned=True,init_return_code=0,init_succeeded=True)
        self.assertTrue(confirmed_catalog_bridge(r))
        r['worker']['init_return_code']=False
        self.assertFalse(confirmed_catalog_bridge(r))

    def test_wrong_source_entry_thread_or_progress_cannot_confirm_texts(self):
        for key,value in [('source_sha256','c'*64),('value_utf8_sha256','c'*64),
                          ('key','10-2'),('thread_id',43),('character_count',0),
                          ('requested_file','other.po')]:
            r=sample();r['worker']['text_catalog_reads'][0][key]=value
            self.assertFalse(confirmed_catalog_bridge(r),(key,value))
        for key,value in [('text_catalog_served_count',0),('text_catalog_sources_unchanged',False),
                          ('text_catalog_map_checked',False),('post_text_catalog_host_request',False),
                          ('text_catalog_error_type','Failure'),('cls_close_completed',False)]:
            r=sample();r['worker'][key]=value
            self.assertFalse(confirmed_catalog_bridge(r),(key,value))
        r=sample();r['catalog_sources_unchanged']=False
        self.assertFalse(confirmed_catalog_bridge(r))


if __name__=='__main__':
    unittest.main()

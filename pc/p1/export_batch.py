"""Read-only workbook/disc export of plain STAGE dialogue; never patch the disc."""
from pathlib import Path
from zipfile import ZipFile
import xml.etree.ElementTree as ET
import collections
import hashlib
import json
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
NS = {'x': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}

def workbook():
    with ZipFile(ROOT / '334.xlsx') as z:
        strings = [''.join(e.itertext()) for e in ET.fromstring(z.read('xl/sharedStrings.xml')).findall('x:si', NS)] if 'xl/sharedStrings.xml' in z.namelist() else []
        def value(c):
            if c.get('t') == 'inlineStr': return ''.join(c.find('x:is', NS).itertext())
            v = c.find('x:v', NS)
            return strings[int(v.text)] if v is not None and c.get('t') == 's' else (v.text if v is not None else '')
        out = {}
        for row in ET.fromstring(z.read('xl/worksheets/sheet1.xml')).findall('x:sheetData/x:row', NS):
            cells = {''.join(filter(str.isalpha, c.get('r'))): value(c) for c in row}
            uid = cells.get('A', '')
            if re.fullmatch(r'U\d+', uid):
                if uid in out: raise ValueError('Duplicate workbook UID: ' + uid)
                out[uid] = cells.get('G', '').replace('\r', '')
        return out

class Disc:
    def __init__(self):
        self.f = open(ROOT / 'ROM/Super Tokusatsu Taisen 2001 (Japan).bin', 'rb')
    def read(self, lba, size):
        chunks = []
        for i in range((size + 2047)//2048):
            self.f.seek((lba+i)*2352+24); chunks.append(self.f.read(2048))
        return b''.join(chunks)[:size]
    def stage(self):
        pvd = self.read(16,2048)
        if pvd[1:6] != b'CD001': raise ValueError('Invalid original disc')
        root = pvd[156:190]; data = self.read(int.from_bytes(root[2:6],'little'),int.from_bytes(root[10:14],'little'))
        p = 0
        while p < len(data):
            n = data[p]
            if not n: p = (p//2048+1)*2048; continue
            r = data[p:p+n]; p += n
            if r[33:33+r[32]].split(b';')[0] == b'STAGE.DAT':
                lba,size = int.from_bytes(r[2:6],'little'),int.from_bytes(r[10:14],'little')
                h = self.read(lba,2048); count = struct.unpack_from('<H',h)[0]
                offsets = list(struct.unpack_from(f'<{count}H',h,2))+[size//2048]
                return lba,offsets
        raise ValueError('STAGE.DAT missing')

def main():
    translations = workbook()
    rows = json.loads((ROOT/'reference/english-toolchain/work/script/ja/strings.json').read_text(encoding='utf-8'))
    disc = Disc(); lba,offsets = disc.stage(); cache = {}; groups = {}; skipped = []; eligible = []
    for row in rows:
        if row['src'] != 'stage': continue
        uid = row['uid']; text = translations.get(uid, '')
        reason = None
        if not text.strip(): reason = 'missing_translation'
        elif re.search(r'[<>\[\]{}]',re.sub(r'\[HERO[1-4]\]','',text.replace('<p>',''))): reason = 'translation_controls_or_variables'
        elif row['term'] not in ('FFFD','FFFE'): reason = 'unsupported_terminal'
        if reason:
            skipped.append({'id':row['id'],'uid':uid,'reason':reason}); continue
        index = int(row['file'][-4:])
        if index not in cache: cache[index] = disc.read(lba+offsets[index],(offsets[index+1]-offsets[index])*2048)
        data = cache[index]; start = row['off']; words = []
        for p in range(start,min(start+2048,len(data)-1),2):
            code = struct.unpack_from('<H',data,p)[0]; words.append(code)
            if code in (0xfffd,0xfffe): break
        if not words or words[-1] != int(row['term'],16): reason = 'terminal_mismatch'
        header = 0
        if words and words[0]==0xff33 and 0xff30 in words:
            header = words.index(0xff30)+1
            if header<len(words) and words[header]==0xfffb: header+=1
        body = words[header:]
        if not reason and any(c>=0xff00 and c not in (0xff01,0xff02,0xff03,0xff04,0xfffb,0xfffc,0xfffd,0xfffe) for c in body): reason = 'source_controls'
        elif not reason and len(text.split('<p>')) != body.count(0xfffc)+1: reason = 'page_count_mismatch'
        elif not reason and any(text.count(f'[HERO{i}]')!=body.count(0xff00+i) for i in range(1,5)): reason='variable_count_mismatch'
        if not reason:
            source_pages=[[]]
            for code in body:
                if code==0xfffc:source_pages.append([])
                else:source_pages[-1].append(code)
            if any(part.count(f'[HERO{i}]')!=source_page.count(0xff00+i) for part,source_page in zip(text.split('<p>'),source_pages) for i in range(1,5)):reason='variable_page_mismatch'
        if reason:
            skipped.append({'id':row['id'],'uid':uid,'reason':reason}); continue
        raw = struct.pack(f'<{len(words)}H',*words)
        if start<128:
            skipped.append({'id':row['id'],'uid':uid,'reason':'insufficient_source_context'}); continue
        row = dict(row, context=data[start-128:start], header=header)
        if len(raw)<8:
            skipped.append({'id':row['id'],'uid':uid,'reason':'short_ambiguous_signature'}); continue
        groups.setdefault(raw,{}).setdefault(text,[]).append(row)
    records = []
    for raw,variants in groups.items():
        contexts={}
        for text,rr in variants.items():
            for r in rr:contexts.setdefault(r['context'],set()).add(text)
        for text,rr in variants.items():
            good=[]
            for r in rr:
                if len(contexts[r['context']])>1:skipped.append({'id':r['id'],'uid':r['uid'],'reason':'conflicting_translation_for_context'})
                else:good.append(r)
            if not good:continue
            uid=good[0]['uid'];records.append((uid,raw,text,sorted({r['context'] for r in good}),good[0]['header']))
            eligible.extend({'id':r['id'],'uid':r['uid']} for r in good)
    records.sort()
    out = ROOT/'pc/p1/data'; out.mkdir(parents=True,exist_ok=True)
    pack = bytearray(b'STTB0005'+struct.pack('<I',len(records)))
    for uid,raw,text,contexts,header in records:
        u,t = uid.encode(),text.encode('utf-8')
        pack += struct.pack('<III',len(u),len(raw),len(t))+u+raw+t
        pack += struct.pack('<II',header,len(contexts))+b''.join(contexts)
    (out/'dialogues.bin').write_bytes(pack)
    import sys
    sys.path.insert(0,str(ROOT/'reference/english-toolchain/tools'))
    from kana_table import TABLE
    mapping={int(k):v for k,v in json.loads((ROOT/'reference/english-toolchain/work/font/variant_maps.json').read_text(encoding='utf-8'))['11'].items()}
    mapping.update(TABLE)
    chars=sorted((k,ord(v)) for k,v in mapping.items() if 0<=k<0xff00 and len(v)==1 and ord(v)<=0xffff)
    (out/'name-glyphs.bin').write_bytes(struct.pack('<I',len(chars))+b''.join(struct.pack('<HI',k,v) for k,v in chars))
    # Preserve the old, strict single-page data for both earlier executables.
    def single_page(r):
        _,raw,text,_,header=r
        words=struct.unpack(f'<{len(raw)//2}H',raw)[header:]
        return not any(c in words for c in (0xff01,0xff02,0xff03,0xff04,0xfffc)) and '<p>' not in text and text.count('\n')==words.count(0xfffb) and text.count('\n')+bool(header)<4
    raw_variants={}
    for r in records:raw_variants.setdefault(r[1],set()).add(r[2])
    previous=[r for r in records if single_page(r) and len(raw_variants[r[1]])==1]
    prior=bytearray(b'STTB0003'+struct.pack('<I',len(previous)))
    for uid,raw,text,contexts,header in previous:
        u,t=uid.encode(),text.encode('utf-8');prior+=struct.pack('<III',len(u),len(raw),len(t))+u+raw+t
        prior+=struct.pack('<II',header,len(contexts))+b''.join(c[-16:] for c in contexts)
    (out/'dialogues-v3.bin').write_bytes(prior)
    legacy_records=[r for r in previous if not r[4]]
    legacy=bytearray(b'STTB0002'+struct.pack('<I',len(legacy_records)))
    for uid,raw,text,contexts,header in legacy_records:
        u,t=uid.encode(),text.encode('utf-8')
        legacy+=struct.pack('<III',len(u),len(raw),len(t))+u+raw+t
        legacy+=struct.pack('<I',len(contexts))+b''.join(c[-16:] for c in contexts)
    (out/'dialogues-legacy.bin').write_bytes(legacy)
    report = {'workbook_sha256':hashlib.sha256((ROOT/'334.xlsx').read_bytes()).hexdigest(),
        'records':len(records),'eligible_occurrences':len(eligible),'eligible_units':len({r['uid'] for r in eligible}),
        'stage5_eligible':[r for r in eligible if ':STAGE0005:' in r['id']],
        'skipped_counts':dict(collections.Counter(r['reason'] for r in skipped)),
        'eligible':eligible,'skipped':skipped,
        'limits':'Offline eligibility only; runtime checks origin and measured line width. Other text engines remain Japanese.'}
    (out/'batch_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('eligible','skipped','stage5_eligible')},ensure_ascii=False))

if __name__ == '__main__': main()

"""Read the current workbook and original disc; emit one local P1 data pack."""
from pathlib import Path
from zipfile import ZipFile
import xml.etree.ElementTree as ET
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_disc import Disc

ns = {'x': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
with ZipFile(ROOT / '334.xlsx') as z:
    strings = []
    if 'xl/sharedStrings.xml' in z.namelist():
        strings = [''.join(e.itertext()) for e in ET.fromstring(z.read('xl/sharedStrings.xml')).findall('x:si', ns)]
    def value(c):
        if c.get('t') == 'inlineStr':
            return ''.join(c.find('x:is', ns).itertext())
        v = c.find('x:v', ns)
        return strings[int(v.text)] if v is not None and c.get('t') == 's' else (v.text if v is not None else '')
    translation = None
    for row in ET.fromstring(z.read('xl/worksheets/sheet1.xml')).findall('x:sheetData/x:row', ns):
        cells = {''.join(filter(str.isalpha, c.get('r'))): value(c) for c in row}
        if cells.get('A') == 'U00208':
            translation = cells.get('G')
            break
if not translation or '<' in translation:
    raise ValueError('Unexpected U00208 translation/control tags')
disc = Disc(ROOT / 'ROM/Super Tokusatsu Taisen 2001 (Japan).bin')
entry = disc.archive_entry(*disc.root_files()['STAGE.DAT'], 5)
start = 0x4AAE
end = start
while end + 2 <= len(entry):
    code = int.from_bytes(entry[end:end + 2], 'little')
    end += 2
    if code == 0xFFFD:
        break
else:
    raise ValueError('Dialogue terminator missing')
out = ROOT / 'pc/p1/data'
out.mkdir(parents=True, exist_ok=True)
(out / 'caption.txt').write_text(translation, encoding='utf-8')
(out / 'signature.bin').write_bytes(entry[start:end])
print(f'U00208: {len(translation)} characters, {end-start} signature bytes')

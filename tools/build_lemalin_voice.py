"""Adapt the proven 2B PCM layout for Le Malin's selected audition clips."""
from pathlib import Path
import hashlib, json, os, shutil, struct, subprocess, wave, zlib
from functools import lru_cache
import imageio_ffmpeg

R = Path(__file__).resolve().parents[1]
TOOLS_2B = Path(os.environ.get('LEMALIN_REFERENCE_ROOT', str(R/'private/reference')))
O = R/'output/voice-prototype/0.1.0-dev.3'
REF = TOOLS_2B/'work/audio-research/reference-battle03-legacy'
META = TOOLS_2B/'work/audio-research/reference-battle03'
SOURCE = R/'output/voice-audition/v1'
OUT = O/'legacy'
OUT.mkdir(parents=True, exist_ok=True)
shutil.copy2(REF/'scriptobjects.bin', OUT/'scriptobjects.bin')
clips = json.loads((SOURCE/'manifest.json').read_text(encoding='utf-8'))['clips']
attack = [clip for clip in clips if clip['id'] in ['A1', 'A2', 'A4']]
damage = [clip for clip in clips if clip['group'] == 'B']
breath = [clip for clip in clips if clip['group'] == 'C']
plans = {
 'Attack_Short': attack, 'ChuZhao_01_DuanCu': attack, 'ChuZhao_02': attack,
 'Charge_Short': attack, 'Charge_Heavy_Short': attack,
 'Charge_Heavy_Long': attack, 'Charge_SuperLong': attack,
 'Charge_Release_Heavy': attack, 'Execute_Attack_Heavy': attack,
 'Hurt': damage, 'Hurt_BiXian': damage,
 'Hurt_Heavy': damage, 'Hurt_Heavy_Mix': damage,
 'ShouZhao_DoubleBlade': breath, 'ShouZhao_LongBlade': breath,
 'ShouZhao_TangBlade': breath, 'XiQi_01': breath,
 'BiQi_01_DuanCu': attack, 'BiQi_02_Long': attack,
 'Charge_Long': attack, 'Charge_Release_Light': attack, 'Heavy_Release': attack,
 'ZhongZhuan_01': None, 'ZhongZhuan': breath,
 'JinGu_Fly_Light': damage, 'JinGu_Fly': damage,
 'JinGu_Pre_Light': damage, 'JinGu_Pre': damage, 'JinGu_QiShen': damage,
 'Special_Stifle': damage, 'Weapon_Break': damage,
 'Break': None, 'Burp_BiXian': None, 'Burp_Short': None, 'Burp': None,
 'Cough': None, 'JinGu_Long': None, 'Snort': None, 'Stand': None,
}

@lru_cache(maxsize=None)
def leveled_pcm(source_file):
    with wave.open(str(SOURCE/source_file), 'rb') as source:
        assert (source.getframerate(), source.getnchannels(), source.getsampwidth()) == (48000, 1, 2)
        pcm = source.readframes(source.getnframes())
    result = subprocess.run([
        imageio_ffmpeg.get_ffmpeg_exe(), '-hide_banner', '-loglevel', 'error',
        '-f', 's16le', '-ar', '48000', '-ac', '1', '-i', 'pipe:0',
        '-af', 'volume=3dB,alimiter=limit=0.95:level=false:attack=1:release=30:latency=true',
        '-f', 's16le', 'pipe:1'], input=pcm, capture_output=True, check=True)
    assert len(result.stdout) == len(pcm), 'Level adjustment changed clip duration'
    return result.stdout

def replace_one(buf, before, after):
    old=struct.pack('<I',before)
    assert buf.count(old)==1, (before,buf.count(old))
    struct.pack_into('<I',buf,buf.index(old),after)

def riff_chunks(data):
    pos=12
    while pos+8<=len(data):
        size=struct.unpack_from('<I',data,pos+4)[0]
        yield data[pos:pos+4],bytes(data[pos+8:pos+8+size])
        pos+=8+size+(size&1)

def rewrite_language(event, language, candidates, resources, resource_start, oldbulk, uexp):
    cooked = language['Value']
    label = language['Key']['LanguageName']
    assert len(cooked['SoundBanks']) == 1
    bank_resource = resources[resource_start]
    bank = bytearray(oldbulk[bank_resource[1]:bank_resource[1] + bank_resource[3]])
    assert bank.startswith(b'BKHD')
    assert struct.unpack_from('<II', bank, 12) == (cooked['SoundBanks'][0]['SoundBankId'], language['Key']['LanguageId'])
    hierarchies = cooked['SoundBanks'][0]['PackagedFile']['BulkData']['Hierarchies']
    sounds = {}
    for node in hierarchies:
        if node['Type'] == 'SoundSfxVoice' and node['Data']['Source']['Plugin']['PluginId'] == 'PCM':
            sounds.setdefault(int(node['Data']['Source']['SourceId']), []).append(node)
    assert set(sounds) == {media['MediaId'] for media in cooked['Media']}, (event, label, [(node['Data']['Source']['SourceId'], node['Data']['Source']['Plugin']['PluginId']) for node in hierarchies if node['Type'] == 'SoundSfxVoice'])
    template_resource = resources[resource_start + 1]
    template = oldbulk[template_resource[1]:template_resource[1] + template_resource[3]]
    fmt = bytearray(next(data for tag, data in riff_chunks(template) if tag == b'fmt '))
    assert len(fmt) == 24 and struct.unpack_from('<HH', fmt) == (0xfffe, 1)
    assert struct.unpack_from('<HH', fmt, 12) == (2, 16)
    struct.pack_into('<II', fmt, 4, 48000, 96000)

    def identity(value):
        return zlib.crc32(('Hcxmmx_LeMalin_voice_0.1.0-dev.3/' + event + '/' + label + '/' + value).encode()) & 0xffffffff

    count_media = len(cooked['Media'])
    voiced_count = 0
    if candidates:
        voiced_count = count_media if candidates is damage else max(1, count_media // 3)
    voiced_slots = {index * count_media // voiced_count for index in range(voiced_count)}
    phase = zlib.crc32(event.encode()) % len(candidates) if candidates else 0
    voiced_index = 0
    rows, payloads = [], []
    for index, media in enumerate(cooked['Media']):
        clip = None
        if index in voiced_slots:
            clip = candidates[(phase + voiced_index) % len(candidates)]
            voiced_index += 1
            audio = leveled_pcm(clip['src'])
        else:
            resource = resources[resource_start + 1 + index]
            original_wem = oldbulk[resource[1]:resource[1] + resource[3]]
            original_pcm = next(data for tag, data in riff_chunks(original_wem) if tag == b'data')
            original_fmt = next(data for tag, data in riff_chunks(original_wem) if tag == b'fmt ')
            original_byte_rate = struct.unpack_from('<I', original_fmt, 8)[0]
            audio = bytes(round(len(original_pcm) / original_byte_rate * 48000) * 2)
        duration = len(audio) / 96000
        body = b'WAVE' + b'fmt ' + struct.pack('<I', len(fmt)) + fmt + b'JUNK' + struct.pack('<I', 4) + bytes(4) + b'data' + struct.pack('<I', len(audio)) + audio
        wem = b'RIFF' + struct.pack('<I', len(body)) + body
        oldid = media['MediaId']
        newid = identity(f'media/{index}')
        newhash = identity(f'payload/{index}/' + hashlib.sha256(wem).hexdigest())
        replace_one(uexp, oldid, newid)
        replace_one(uexp, media['PackagedFile']['Hash'], newhash)
        for sound in sounds[oldid]:
            source = sound['Data']['Source']
            oldrecord = struct.pack('<IIBIII', int(sound['Id']), 0x00010001, 0, oldid, source['CacheId'], source['InMemoryMediaSize'])
            assert bank.count(oldrecord) == 1
            struct.pack_into('<III', bank, bank.index(oldrecord) + 9, newid, identity(f'cache/{index}'), len(wem))
        payloads.append(wem)
        rows.append({'language': label, 'slot': index + 1, 'clip_id': clip['id'] if clip else 'SILENCE',
                     'source_file': clip['src'] if clip else None, 'duration_seconds': duration,
                     'media_id': newid, 'packaged_hash': newhash})
    bank[32:48] = hashlib.md5(bank[:32] + bytes(16) + bank[48:]).digest()
    replace_one(uexp, cooked['SoundBanks'][0]['PackagedFile']['Hash'], identity('bank/' + hashlib.sha256(bank).hexdigest()))
    return [bytes(bank), *payloads], rows


def build_event(event, candidates):
    path = Path('ProjectPBZ/Content/Wwise/Events/VO/Int/Hun/Battle') / event
    meta = json.loads((META / (event + '.json')).read_text(encoding='utf-8'))[0]
    # Chinese branches use selected clips or silence. Embedded English branches
    # retain their native codec and bytes, matching the prior language scope.
    languages = [entry for entry in meta['EventCookedData']['EventLanguageMap']
                 if entry['Value']['SoundBanks'][0]['PackagedFile'].get('BulkData')]
    count = sum(1 + len(entry['Value']['Media']) for entry in languages)
    uasset = bytearray((REF / path.with_suffix('.uasset')).read_bytes())
    uexp = bytearray((REF / path.with_suffix('.uexp')).read_bytes())
    oldbulk = (REF / path.with_suffix('.ubulk')).read_bytes()
    marker = struct.pack('<II', 1, count)
    assert uasset.count(marker) == 1
    map_start = uasset.index(marker)
    assert map_start + 8 + count * 44 <= len(uasset)
    resources = [list(struct.unpack_from('<IqqqqII', uasset, map_start + 8 + index * 44)) for index in range(count)]
    payloads, rows = [], []
    for language in languages:
        if language['Key']['LanguageName'] != 'Chinese':
            start = len(payloads)
            count_language = 1 + len(language['Value']['Media'])
            for resource in resources[start:start + count_language]:
                payloads.append(oldbulk[resource[1]:resource[1] + resource[3]])
            continue
        rewritten, selected = rewrite_language(event, language, candidates, resources, len(payloads), oldbulk, uexp)
        payloads.extend(rewritten)
        rows.extend(selected)
    struct.pack_into('<ff', uexp, len(uexp) - 16, max(row['duration_seconds'] for row in rows), min(row['duration_seconds'] for row in rows))
    offset = 0
    newbulk = bytearray()
    for index, payload in enumerate(payloads):
        entry = resources[index]
        entry[1] = offset
        entry[3] = entry[4] = len(payload)
        struct.pack_into('<IqqqqII', uasset, map_start + 8 + index * 44, *entry)
        newbulk.extend(payload)
        offset += len(payload)
    target = OUT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    for extension, data in [('.uasset', uasset), ('.uexp', uexp), ('.ubulk', newbulk)]:
        target.with_suffix(extension).write_bytes(data)
    return {'event': event, 'slots': rows}


mapping=[]
for p in sorted(META.glob('SFX*.json')):
    suffix=p.stem.split('Hun_',1)[-1]
    if suffix in plans:mapping.append(build_event(p.stem,plans[suffix]))
assert len(mapping)==len(plans),(len(mapping),len(plans))
(O/'clip-mapping.json').write_text(json.dumps(mapping,ensure_ascii=False,indent=2),encoding='utf-8')
stage=O/'staging';paks=stage/'ProjectPBZ/Content/Paks/~mods/HcxmmxLeMalinVoice';paks.mkdir(parents=True,exist_ok=True)
name='zzz_HcxmmxLeMalinVoice_0_1_0_dev_3_P'
with (O/'pack.log').open('w',encoding='utf-8') as log:
    subprocess.run([os.environ.get('RETOC', str(TOOLS_2B/'tools/retoc/package/retoc.exe')),'to-zen',str(OUT),str(paks/(name+'.utoc')),'--version','UE5_7'],stdout=log,stderr=subprocess.STDOUT,check=True)
files = [{'name': file.name, 'size': file.stat().st_size,
          'sha256': hashlib.sha256(file.read_bytes()).hexdigest()}
         for file in sorted(paks.glob(name + '.*'))]
assert {Path(file['name']).suffix for file in files} == {'.pak', '.utoc', '.ucas'}
(O/'package.json').write_text(json.dumps({'version':'0.1.0-dev.3', 'directory':str(paks),
                                        'files':files}, indent=2),encoding='utf-8')
print('PACKAGED',len(mapping),'events',sum(len(x['slots']) for x in mapping),'slots')
print('VOICED CLIPS',sorted({slot['clip_id'] for row in mapping for slot in row['slots']}))

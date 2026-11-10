"""Prepare a small, reversible audition using indexed meaning and silence gaps."""
from pathlib import Path
import json
import shutil

import numpy as np
from scipy.signal import resample_poly
import soundfile as sf

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'work/voice-research'
OUT = ROOT / 'output/voice-audition/v1'
RATE = 48000
groups = [
    {'id': 'A', 'name': '攻击候选', 'note': '战斗短句与末段片段，先判断语气和长度。'},
    {'id': 'B', 'name': '受击／闪避候选', 'note': '包含惊呼，适配用途由试听决定。'},
    {'id': 'C', 'name': '呼吸／待命候选', 'note': '来自哈欠与睡息，偏慵懒；试听判断是否适合回气或待命。'},
]
# Seconds in the unmodified recording. These are first-pass audition cuts.
cuts = [
    ('A1', '901110', 'skill', .055, .82, '日文原文「消えろ！！」，意为消失吧／滚开。完整技能短句。'),
    ('A2', '901110', 'battle', 2.41, 3.20, '原台词末句「参ります！」，意为我上了／出击。按原文与切点对应，音频转写待确认。'),
    ('A3', '901110', 'battle_ex', 1.78, 2.68, '原句「アイリスの心は、私に！」，后半句「私に」意为在我这里／寄于我。剪辑缺上文，建议撤出攻击候选。音频转写待确认。'),
    ('A4', '901110', 'lose', 2.96, 3.82, '原台词末句「まだよ！！」，意为还没完。按原文与切点对应，音频转写待确认。'),
    ('B1', '901110', 'hp_warning', .065, 1.10, '低血量短句，尝试较重受击。'),
    ('B2', '901110', 'touch2', .11, .91, '基础特殊触摸的句首惊呼。'),
    ('B3', '901111', 'touch2', .065, .89, '居家皮肤的句首惊呼。'),
    ('B4', '901113', 'touch2', .11, 1.50, '兔女郎皮肤的句首惊呼，时长较长。'),
    ('B5', '901110', 'touch', 5.23, 6.16, '基础触摸台词中间的惊呼。'),
    ('C1', '901110', 'detail', .11, 1.17, '详情台词句首，哈欠／呼吸候选。'),
    ('C2', '901110', 'feeling1', .065, 1.70, '睡息第一段，偏待命。'),
    ('C3', '901110', 'detail', 8.41, 9.88, '详情台词中段睡息，偏待命。'),
]
manifest = json.loads((SOURCE / 'download-manifest.json').read_text(encoding='utf-8'))
by_key = {(clip['skin_id'], clip['key']): clip for clip in manifest['clips']}
for folder in ['clips', 'originals']:
    (OUT / folder).mkdir(parents=True, exist_ok=True)
clips = []
for number, skin_id, key, start, end, note in cuts:
    source = SOURCE / 'audio' / skin_id / (key + '.ogg')
    audio, rate = sf.read(source, dtype='float32')
    assert rate == 44100 and audio.ndim == 1 and 0 <= start < end <= len(audio) / rate
    selected = audio[round(start * rate):round(end * rate)].copy()
    selected -= selected.mean()
    selected = resample_poly(selected, 160, 147)
    rms = float(np.sqrt(np.mean(selected ** 2)))
    peak = float(np.max(np.abs(selected)))
    assert peak > .01
    gain = min(.12 / max(rms, 1e-6), .80 / peak, 2.)
    selected *= gain
    fade_in, fade_out = round(.005 * RATE), round(.015 * RATE)
    selected[:fade_in] *= np.linspace(0, 1, fade_in)
    selected[-fade_out:] *= np.linspace(1, 0, fade_out)
    sf.write(OUT / 'clips' / (number + '.wav'), selected, RATE, subtype='PCM_16')
    original_name = skin_id + '_' + key + '.ogg'
    shutil.copy2(source, OUT / 'originals' / original_name)
    clips.append({
        'id': number, 'group': number[0], 'skin_id': skin_id, 'key': key,
        'start': start, 'end': end, 'seconds': round(len(selected) / RATE, 3),
        'gain_db': round(20 * np.log10(gain), 2), 'note': note,
        'src': 'clips/' + number + '.wav', 'original': 'originals/' + original_name,
        'source_sha256': by_key[(skin_id, key)]['sha256'],
        'source_url': by_key[(skin_id, key)]['pinned_url'],
    })

for group in groups:
    parts, cursor = [], 0
    for clip in [item for item in clips if item['group'] == group['id']]:
        samples, rate = sf.read(OUT / clip['src'], dtype='float32')
        clip['batch_start'] = cursor / RATE
        clip['batch_end'] = (cursor + len(samples)) / RATE
        parts.extend([samples, np.zeros(round(.45 * RATE), dtype='float32')])
        cursor += len(samples) + round(.45 * RATE)
    group['src'] = 'group-' + group['id'] + '.wav'
    sf.write(OUT / group['src'], np.concatenate(parts), RATE, subtype='PCM_16')
    group['seconds'] = round(cursor / RATE, 3)

data = {'schema': 'lemalin-audition-v1', 'clips': clips, 'groups': groups,
        'cut_status': 'Initial cuts from index context and silence gaps; listening selection pending.'}
(OUT / 'manifest.json').write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')
template = (ROOT / 'tools/lemalin_voice_audition.html').read_text(encoding='utf-8')
page = template.replace('__DATA__', json.dumps(data, ensure_ascii=False).replace('</', '<\\/'))
(OUT / '试听.html').write_text(page, encoding='utf-8')
print('Prepared', len(clips), 'clips;', round(sum(clip['seconds'] for clip in clips), 2), 'seconds')
print(OUT / '试听.html')

"""Fetch and decode the indexed Le Malin recordings without changing the game."""
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from pathlib import Path
from urllib.request import Request, urlopen
import hashlib
import json

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[1] / 'work/voice-research'
REVISION = '21366bf104dccc515d084b33d61d98ff994dc7f2'
index = json.loads((ROOT.parents[1] / 'metadata/voice-index.json').read_text(encoding='utf-8'))


def fetch_clip(item):
    skin_id, key, url = item
    target = ROOT / 'audio' / skin_id / (key + '.ogg')
    target.parent.mkdir(parents=True, exist_ok=True)
    pinned_url = url.replace('/main/', '/' + REVISION + '/')
    for attempt in range(2):
        try:
            if not target.exists() or attempt:
                request = Request(pinned_url, headers={'User-Agent': 'LeMalin-resource-research'})
                with urlopen(request, timeout=60) as response:
                    payload = response.read()
                target.write_bytes(payload)
            info = sf.info(str(target))
            samples, rate = sf.read(str(target), dtype='float32', always_2d=True)
            assert len(samples) == info.frames and len(samples) > 0
            assert np.isfinite(samples).all()
            return {
                'skin_id': skin_id, 'key': key, 'source_url': url,
                'pinned_url': pinned_url, 'file': target.relative_to(ROOT).as_posix(),
                'bytes': target.stat().st_size,
                'sha256': hashlib.sha256(target.read_bytes()).hexdigest(),
                'pcm_sha256': hashlib.sha256(samples.tobytes()).hexdigest(),
                'format': info.format, 'codec': info.subtype,
                'sample_rate': rate, 'channels': info.channels,
                'duration_seconds': round(info.duration, 4),
                'peak': round(float(np.max(np.abs(samples))), 5),
                'decode': 'complete',
            }
        except Exception as error:
            failure = str(error)
    return {'skin_id': skin_id, 'key': key, 'source_url': url, 'error': failure}


items = [(skin_id, key, url) for skin_id, skin in index.items()
         for key, url in skin['voicelinks'].items() if url]
with ThreadPoolExecutor(max_workers=6) as pool:
    clips = list(pool.map(fetch_clip, items))
summary = []
for skin_id, skin in index.items():
    selected = [clip for clip in clips if clip['skin_id'] == skin_id]
    summary.append({'id': skin_id, 'name': skin['skin']['name'],
                    'indexed': len(skin['voicelines']),
                    'downloaded_decoded': sum(clip.get('decode') == 'complete' for clip in selected)})
report = {'checked_utc': datetime.now(timezone.utc).isoformat(), 'repository_revision': REVISION,
          'skins': summary, 'clips': clips}
(ROOT / 'download-manifest.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
for row in summary:
    print(row)
good = [clip for clip in clips if clip.get('decode') == 'complete']
print('TOTAL', len(good), '/', len(clips), 'BYTES', sum(clip['bytes'] for clip in good),
      'SECONDS', round(sum(clip['duration_seconds'] for clip in good), 2),
      'UNIQUE_PCM', len({clip['pcm_sha256'] for clip in good}))
print('FORMATS', sorted({(clip['format'], clip['codec'], clip['sample_rate'], clip['channels']) for clip in good}))
for clip in clips:
    if clip.get('error'):
        print('ERROR', clip['skin_id'], clip['key'], clip['error'])
    elif clip['key'] in ['battle', 'skill', 'hp_warning', 'lose', 'win_mvp', 'battle_ex']:
        print('PRIORITY', clip['skin_id'], clip['key'], clip['duration_seconds'])

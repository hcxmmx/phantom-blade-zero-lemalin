"""Bundle the accepted character and optional voice builds for manual installation."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import tempfile
import zipfile


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, required=True,
                        help='Local production workspace containing the accepted output builds')
    parser.add_argument('--destination', type=Path)
    args = parser.parse_args()
    workspace = args.workspace.resolve()
    destination = (args.destination or workspace / 'output/releases').resolve()
    destination.mkdir(parents=True, exist_ok=True)
    repository = Path(__file__).resolve().parents[1]
    instructions = repository / 'docs/玩家说明.md'
    builds = [
        ('LeMalin-0.2.0-Player.zip', '0.2.0-dev.30',
         workspace / 'output/game-0.2.0-dev.30/manifest.json',
         workspace / 'output/game-0.2.0-dev.30', 'HcxmmxLeMalin'),
        ('LeMalin-Voice-0.1.0.zip', '0.1.0-dev.3',
         workspace / 'output/voice-prototype/0.1.0-dev.3/package.json',
         workspace / 'output/voice-prototype/0.1.0-dev.3/staging/ProjectPBZ/Content/Paks/~mods/HcxmmxLeMalinVoice',
         'HcxmmxLeMalinVoice'),
    ]
    packages = []
    for filename, build, manifest_path, source, folder in builds:
        records = json.loads(manifest_path.read_text(encoding='utf-8-sig'))['files']
        assert len(records) == 3
        assert {Path(item['name']).suffix for item in records} == {'.pak', '.utoc', '.ucas'}
        for item in records:
            assert Path(item['name']).name == item['name']
            path = source / item['name']
            assert path.stat().st_size == item['size'], path
            assert sha256(path) == item['sha256'], path
        target = destination / filename
        with tempfile.NamedTemporaryFile(dir=destination, suffix='.partial', delete=False) as handle:
            temporary = Path(handle.name)
        try:
            with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
                archive.write(instructions, '使用说明.md')
                for item in records:
                    archive.write(source / item['name'],
                                  f'ProjectPBZ/Content/Paks/~mods/{folder}/{item["name"]}')
            os.replace(temporary, target)
        finally:
            temporary.unlink(missing_ok=True)
        packages.append({'name': filename, 'source_build': build,
                         'bytes': target.stat().st_size, 'sha256': sha256(target), 'files': records})
        print(f'Packaged {filename}: {target.stat().st_size:,} bytes', flush=True)
    (destination / 'SHA256SUMS.txt').write_text(
        ''.join(f'{item["sha256"]}  {item["name"]}\n' for item in packages), encoding='utf-8')
    (destination / 'release-manifest.json').write_text(
        json.dumps({'packages': packages}, ensure_ascii=False, indent=2), encoding='utf-8')
    print(destination, flush=True)


if __name__ == '__main__':
    main()

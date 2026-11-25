"""Build or verify the self-contained TOPO source ZIP for TE Builder."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path, nargs='?', default=root / 'release' / f"TOPO-{(root / 'userwt/VERSION').read_text().strip()}.zip")
parser.add_argument('--check', action='store_true')
args = parser.parse_args()

manifest = root / 'machine-bundle.json'
bundle = json.loads(manifest.read_bytes())
files = {manifest.name: manifest.read_bytes()}
for path in (root / bundle['machine']).rglob('*'):
    rel = path.relative_to(root)
    if path.is_file() and not any(part.startswith('.') or part == '__pycache__' for part in rel.parts):
        files[rel.as_posix()] = path.read_bytes()
for patch in bundle['patches']:
    data = (root / patch['path']).read_bytes()
    if hashlib.sha256(data).hexdigest() != patch['sha256']:
        parser.error('SDK patch differs from its manifest: ' + patch['path'])
    files[patch['path']] = data
for name in ('README.md', 'LICENSE', 'CHANGELOG.md', 'CONTRIBUTING.md'):
    files[name] = (root/name).read_bytes()
for folder in ('LICENSES', 'tools', 'tests', 'docs', 'examples'):
    for path in sorted((root / folder).iterdir()):
        if path.is_file() and not path.name.startswith('.'):
            files[path.relative_to(root).as_posix()] = path.read_bytes()

if args.check:
    with ZipFile(args.output) as archive:
        if sorted(archive.namelist()) != sorted(files) or any(archive.read(name) != data for name, data in files.items()):
            parser.error('Build bundle differs from the checkout')
else:
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.output.parent) as scratch:
        staged = Path(scratch) / args.output.name
        with ZipFile(staged, 'w') as archive:
            for name, data in sorted(files.items()):
                entry = ZipInfo(name)
                entry.external_attr = 0o100644 << 16
                archive.writestr(entry, data, compress_type=ZIP_DEFLATED)
        staged.replace(args.output)
if args.output.parent.resolve() == (root / 'release').resolve():
    sums = args.output.parent / 'SHA256SUMS'
    contents = ''.join(hashlib.sha256(path.read_bytes()).hexdigest() + '  ' + path.name + '\n'
                       for path in sorted(args.output.parent.glob('TOPO-*.zip')))
    if args.check:
        if not sums.is_file() or sums.read_text() != contents:
            parser.error('Release checksums differ from the archives')
    else:
        sums.write_text(contents)
print(args.output)

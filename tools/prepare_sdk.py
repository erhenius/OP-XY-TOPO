"""Apply TOPO's checked patches to a clean, disposable Open-XY checkout."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('sdk', type=Path)
args = parser.parse_args()
sdk = args.sdk.resolve()
manifest = json.loads((root / 'machine-bundle.json').read_bytes())
git = ['git', '-C', str(sdk)]
head = subprocess.check_output(git + ['rev-parse', 'HEAD'], text=True).strip()
if head != manifest['sdkCommit']:
    parser.error('Check out Open-XY at ' + manifest['sdkCommit'])
if subprocess.check_output(git + ['status', '--porcelain', '--untracked-files=all']):
    parser.error('Use a clean disposable SDK checkout')
patches = []
for patch in manifest['patches']:
    path = root / patch['path']
    if hashlib.sha256(path.read_bytes()).hexdigest() != patch['sha256']:
        parser.error('Checksum mismatch for ' + path.name)
    patches.append(str(path))
# Git checks all hunks before updating the checkout.
subprocess.run(git + ['apply', '--whitespace=nowarn', *patches], check=True)

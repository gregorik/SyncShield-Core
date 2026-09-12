"""Create and verify the Core source distribution. Requires Python 3.9+."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=ROOT / 'Artifacts')
args = parser.parse_args()

descriptor = json.loads((ROOT / 'SyncShield.uplugin').read_text(encoding='utf-8-sig'))
assert descriptor['VersionName'] == '0.1' and descriptor['Version'] == 1
assert descriptor['EngineVersion'] == '5.7.0'
assert descriptor['CanContainContent'] is False
assert descriptor['SupportedTargetPlatforms'] == ['Win64']
assert len(descriptor['Modules']) == 1
assert descriptor['Modules'][0]['Type'] == 'Editor'
assert descriptor['Modules'][0]['PlatformAllowList'] == ['Win64']

matrix = (ROOT / 'docs/FEATURES.md').read_text(encoding='utf-8')
rows = re.findall(r'^\| \d+ \| .+? \| (Yes|No) \| Yes \|$', matrix, re.MULTILINE)
assert len(rows) == 24 and rows.count('Yes') == 12, 'Feature matrix must describe 12/24 capabilities'

files = [ROOT / name for name in ('SyncShield.uplugin', 'LICENSE', 'README.md', 'CHANGELOG.md')]
allowed_extensions = {'.cpp', '.h', '.cs', '.png', '.ini', '.md', '.ps1', '.py'}
for directory in ('Source', 'Resources', 'Config', 'docs', 'Scripts'):
    for path in (ROOT / directory).rglob('*'):
        if path.is_file():
            assert not path.is_symlink(), f'Unexpected link: {path}'
            assert path.suffix in allowed_extensions, f'Unexpected payload: {path}'
            files.append(path)

commercial_symbols = (
    'FSyncShieldHistoryService', 'FSyncShieldValidationService', 'FSyncShieldDemo',
    'SaveRecentlyTouched', 'UpdateConflictSentinel', 'RemoteChangeMatchesPackageFile',
    'BuildBranchCheckoutArgs', 'ExecuteSafeBranchShift', 'ExecuteGitCommit',
    'ExecuteGitStash', 'ExecuteGitMergeAbort', 'ExecuteGitRebaseAbort',
    'ReleaseAllLfsLocks', 'UnlockLfsFilesAsync', 'bEnableLocalSaveHistory',
    'bEnablePreSaveValidation', 'bEnableDemoContentCommands',
)
for path in files:
    if path.suffix in {'.cpp', '.h', '.cs'}:
        source = path.read_text(encoding='utf-8-sig')
        assert not any(symbol in source for symbol in commercial_symbols), f'Commercial capability leaked into {path}'

args.output.mkdir(parents=True, exist_ok=True)
archive = args.output / 'SyncShield-Core-0.1-UE5.7-source.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as output:
    for path in sorted(files):
        info = zipfile.ZipInfo('SyncShield/' + path.relative_to(ROOT).as_posix(), date_time=(2026, 9, 12, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        output.writestr(info, path.read_bytes())

with zipfile.ZipFile(archive) as package:
    assert package.testzip() is None, 'ZIP integrity failure'
    assert len(package.namelist()) == len(set(package.namelist())) == len(files)
    for path in files:
        assert package.read('SyncShield/' + path.relative_to(ROOT).as_posix()) == path.read_bytes()
    assert 'SyncShield/Resources/Icon128.png' in package.namelist()
    assert 'SyncShield/Config/FilterPlugin.ini' in package.namelist()

digest = hashlib.sha256(archive.read_bytes()).hexdigest()
(args.output / 'SHA256SUMS.txt').write_text(f'{digest}  {archive.name}\n', encoding='utf-8')
print(f'PASS: {len(files)} files, 12/24 capabilities, verified ZIP contents, SHA-256 {digest}')
print(archive)

#!/usr/bin/env python3
"""Package only allowlisted source, firmware and upstream notices. Never includes backups."""
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    idf = Path(sys.argv[1]).resolve()
    version = (ROOT / 'VERSION').read_text().strip()
    output = ROOT / 'release'
    output.mkdir(exist_ok=True)
    archive = output / f'open-mind-lamp-{version}-candidate.tar.gz'
    files = {}
    for name in ('.github', 'firmware', 'web', 'analysis', 'tools', 'hardware', 'docs', 'examples'):
        for path in (ROOT / name).rglob('*'):
            rel = path.relative_to(ROOT)
            if path.is_file() and not path.is_symlink() and not any(part in ('build', 'managed_components', '__pycache__', 'test-results') for part in rel.parts):
                if rel.as_posix() not in ('firmware/sdkconfig', 'firmware/sdkconfig.old') and path.suffix not in ('.pyc', '.log'):
                    files[f'source/{rel}'] = path
    for name in ('.gitignore', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'README.md', 'VERSION', 'CHANGELOG.md', 'CONTRIBUTING.md', 'SECURITY.md'):
        files[f'source/{name}'] = ROOT / name
    build = ROOT / 'firmware/build'
    for name in ('open-mind-lamp.bin', 'open-mind-lamp.elf', 'open-mind-lamp.map',
                 'bootloader/bootloader.bin', 'partition_table/partition-table.bin'):
        files[f'firmware/{name}'] = build / name
    files['firmware/sdkconfig'] = ROOT / 'firmware/sdkconfig'
    files['sbom.spdx.json'] = output / 'sbom.spdx.json'
    files['python-tools.txt'] = output / 'python-tools.txt'
    # Include actual upstream notice/license texts, retaining source-relative paths.
    for base, prefix in ((idf, 'esp-idf'), (ROOT / 'firmware/managed_components', 'managed-components')):
        for directory, dirs, names in os.walk(base):
            dirs[:] = [d for d in dirs if d not in ('.git', '__pycache__', 'build', 'node_modules')]
            for name in names:
                upper = name.upper()
                if upper.startswith(('LICENSE', 'LICENCE', 'COPYING', 'NOTICE', 'COPYRIGHT')):
                    path = Path(directory) / name
                    if path.is_file() and not path.is_symlink():
                        files[f'third-party/{prefix}/{path.relative_to(base)}'] = path
    metadata = dict(version=version, status='hardware-validation-candidate', hardware_qualified=False,
                    idf_commit=subprocess.check_output(['git', '-C', str(idf), 'rev-parse', 'HEAD'], text=True).strip(),
                    qualification='See source/docs/validation.md; physical checks remain outstanding.',
                    flash_offsets={'bootloader/bootloader.bin': '0x0', 'partition_table/partition-table.bin': '0x8000', 'open-mind-lamp.bin': '0x10000'})
    hashes = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in sorted(files.items())}
    # Fixed archive timestamps make repackaging identical build inputs reproducible.
    with archive.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as gz, tarfile.open(fileobj=gz, mode='w') as tar:
        for name, path in sorted(files.items()):
            data = path.read_bytes(); info = tarfile.TarInfo(name); info.size = len(data)
            info.mode = 0o755 if os.access(path, os.X_OK) else 0o644
            tar.addfile(info, io.BytesIO(data))
        for name, value in (('manifest.json', metadata), ('file-sha256.json', hashes)):
            data = (json.dumps(value, indent=2, sort_keys=True) + '\n').encode()
            info = tarfile.TarInfo(name); info.size = len(data); info.mode = 0o644
            tar.addfile(info, io.BytesIO(data))
    (output / 'SHA256SUMS').write_text(f'{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n')
    print(f'Candidate bundle: {archive} ({archive.stat().st_size} bytes; {len(files)} source/artifact/notice files)')
    print('Hardware qualification remains pending; bundle is not published automatically.')


if __name__ == '__main__': main()

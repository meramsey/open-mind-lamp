#!/usr/bin/env python3
"""Validate the candidate's member hashes and private-directory exclusions without extraction."""
import hashlib
import json
from pathlib import PurePosixPath
import sys
import tarfile


def main():
    with tarfile.open(sys.argv[1], 'r:gz') as archive:
        members = archive.getmembers()
        names = [m.name for m in members]
        if len(names) != len(set(names)):
            raise ValueError('Duplicate archive members')
        for member in members:
            path = PurePosixPath(member.name)
            if not member.isfile() or path.is_absolute() or any(p in ('..', 'backups', '.git', '.tools', '__pycache__') for p in path.parts):
                raise ValueError(f'Unexpected/private archive path: {member.name}')
        manifest = json.load(archive.extractfile('manifest.json'))
        hashes = json.load(archive.extractfile('file-sha256.json'))
        if manifest.get('hardware_qualified') is not False:
            raise ValueError('Candidate must explicitly record pending hardware validation')
        if set(hashes) != set(names) - {'manifest.json', 'file-sha256.json'}:
            raise ValueError('File hash inventory does not match archive')
        for name, digest in hashes.items():
            if hashlib.sha256(archive.extractfile(name).read()).hexdigest() != digest:
                raise ValueError(f'Hash mismatch: {name}')
        for required in ('source/LICENSE', 'source/THIRD_PARTY_NOTICES.md', 'source/docs/validation.md',
                         'firmware/open-mind-lamp.bin', 'firmware/bootloader/bootloader.bin',
                         'firmware/partition_table/partition-table.bin', 'sbom.spdx.json', 'python-tools.txt'):
            if required not in hashes:
                raise ValueError(f'Missing release artifact: {required}')
        print(f'Verified candidate: {len(hashes)} file hashes; source, binaries, notices and SBOM present; private data excluded.')


if __name__ == '__main__': main()

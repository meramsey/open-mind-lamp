#!/usr/bin/env python3
"""USB identity and permissions only; never writes flash or trusts a USB ID as PCB ID."""
import argparse
import os
import subprocess
import sys
from serial.tools import list_ports


def select_port():
    override = os.environ.get('OML_PORT')
    ports = [p for p in list_ports.comports() if (p.vid, p.pid) == (0x303A, 0x1001)]
    if override:
        matches = [p for p in ports if os.path.realpath(p.device) == os.path.realpath(override)]
    else:
        matches = ports
    if len(matches) != 1:
        raise RuntimeError('Connect exactly one native Espressif USB device, or set OML_PORT to its /dev/serial/by-id path.')
    port = matches[0]
    if not os.access(port.device, os.R_OK | os.W_OK):
        raise PermissionError(f'{port.device}: no serial access. Run sudo usermod -aG dialout "$USER", log out fully, then log in. Do not chmod 666.')
    return port


def probe(port):
    result = subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'auto', '--port', port.device, 'flash-id'],
                            text=True, capture_output=True, timeout=40)
    if result.returncode or 'Chip type:          ESP32-S3 ' not in result.stdout or 'Detected flash size: 4MB' not in result.stdout:
        raise RuntimeError(f'Expected ESP32-S3 and 4 MB flash. Probe output:\n{result.stdout}\n{result.stderr}')
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port-only', action='store_true')
    parser.add_argument('--probe', action='store_true')
    parser.add_argument('--identity-only', action='store_true')
    parser.add_argument('--allow-missing', action='store_true')
    args = parser.parse_args()
    try:
        port = select_port()
        if args.identity_only:
            print(port.serial_number)
        elif args.port_only:
            print(port.device)
        else:
            print(f'{port.device}: USB {port.vid:04x}:{port.pid:04x}, serial {port.serial_number}; permissions OK')
            print('PCB model/revision must be checked on silkscreen; USB identity is not sufficient.')
            if args.probe:
                print(probe(port))
    except (RuntimeError, PermissionError, subprocess.TimeoutExpired) as error:
        print(str(error), file=sys.stderr)
        return 0 if args.allow_missing else 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

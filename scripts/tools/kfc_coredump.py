"""ESP32 crash of a KFC firmware device: summary, download, stack trace and erase.

The firmware serves the crash summary at /savecrash.json, the raw ESP-IDF core dump at /coredump.bin and
erases the dump with /savecrash.json?cmd=erase-coredump. All three need the WebUI session id, which is
computed here the same way as in scripts/libs/kfcfw/session.py and session.cpp. The WebUI user name is the
device name (KFC + the last 3 bytes of the MAC, e.g. KFC0A4271), the default password is 12345678.

`trace` is the workflow behind a crash report: it downloads the dump, finds the archive in elf/ whose
firmware.elf is the firmware of the dump (the summary carries the first 16 hex of the ELF sha256), decodes
the dump with esp-coredump and the symbols of exactly that ELF, resolves the panic addresses with
addr2line and writes everything to logs/:

    logs/coredump_<env>_<build>.bin   the raw dump as downloaded (20 byte flash header + ELF)
    logs/coredump_<env>_<build>.elf   the same without the flash header, for gdb
    logs/crash_<env>_<build>.json     the crash summary of the device
    logs/crash_<env>_<build>.txt      the esp-coredump output plus the resolved addresses

Usage:
    python scripts/tools/kfc_coredump.py trace    --host 192.168.0.196 -u KFC0A4271 -p 12345678
    python scripts/tools/kfc_coredump.py trace    --host 192.168.0.196 -u KFC0A4271 -p 12345678 --erase
    python scripts/tools/kfc_coredump.py trace    --dump logs/coredump_wled_15287.bin --archive elf/wled_esp32_controller_15287.tar.gz
    python scripts/tools/kfc_coredump.py info     --host 192.168.0.196 -u KFC0A4271 -p 12345678
    python scripts/tools/kfc_coredump.py download --host 192.168.0.196 -u KFC0A4271 -p 12345678 -o dump.bin
    python scripts/tools/kfc_coredump.py erase    --host 192.168.0.196 -u KFC0A4271 -p 12345678
"""

import argparse
import glob
import hashlib
import json
import os
import re
import secrets
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.error
import urllib.request

ARCHIVE_NAME = re.compile(r'^(?P<env>.+)_(?P<build>\d+)\.tar\.gz$')
ADDRESS = re.compile(r'0x[0-9a-fA-F]{8}')
ELF_MAGIC = b'\x7fELF'


def project_directory():
    """kfc_fw project directory, this file lives in <project>/scripts/tools."""
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def session_id(username, password, rounds=1024):
    """WebUI session id, mirrors scripts/libs/kfcfw/session.py and session.cpp."""
    salt = secrets.token_bytes(8)
    digest = hashlib.sha256(salt + password.encode('utf8') + username.encode('utf8')).digest()
    for _ in range(rounds):
        digest = hashlib.sha256(digest + salt).digest()
    return salt.hex() + digest.hex()


def fetch(host, path, query=''):
    url = 'http://%s%s' % (host, path)
    if query:
        url += ('&' if '?' in path else '?') + query
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.status, response.read()


def hash_prefix(stream, length=16):
    """First 16 hex of the sha256 of an open stream, the value the crash summary carries."""
    digest = hashlib.sha256()
    for block in iter(lambda: stream.read(1024 * 1024), b''):
        digest.update(block)
    return digest.hexdigest()[:length]


def find_archive(archive_dir, prefix):
    """elf/<env>_<build>.tar.gz whose firmware.elf is this firmware (sha256 prefix) or None."""
    for path in sorted(glob.glob(os.path.join(archive_dir, '*.tar.gz'))):
        try:
            with tarfile.open(path) as archive:
                if hash_prefix(archive.extractfile(archive.getmember('firmware.elf'))) == prefix:
                    return path
        except (tarfile.TarError, KeyError, OSError) as error:
            print('WARNING: cannot read %s: %s' % (path, error), file=sys.stderr)
    return None


def find_tool(bin_dir, suffix):
    """<prefix>-gdb / <prefix>-addr2line of the xtensa toolchain, Windows adds .exe."""
    for path in sorted(glob.glob(os.path.join(bin_dir, '*' + suffix + '*'))):
        if os.path.basename(path).endswith((suffix, suffix + '.exe')):
            return path
    return None


def toolchain_directory():
    """bin directory of the PlatformIO xtensa toolchain (gdb, addr2line) or None."""
    core = os.environ.get('PLATFORMIO_CORE_DIR') or os.path.join(os.path.expanduser('~'), '.platformio')
    for directory in sorted(glob.glob(os.path.join(core, 'packages', 'toolchain-xtensa*', 'bin'))):
        if find_tool(directory, '-gdb'):
            return directory
    return None


def strip_flash_header(data):
    """(header size, ELF core dump) of a downloaded dump, the flash header is 20 bytes."""
    offset = data.find(ELF_MAGIC)
    if offset < 0:
        raise ValueError('no ELF core dump in the downloaded file')
    return offset, data[offset:]


def is_code(address):
    """ESP32 code lives in 0x40000000..0x40500000, a data address has no symbol."""
    return 0x40000000 <= int(address, 16) < 0x40500000


def panic_addresses(text):
    """Code addresses of the panic path itself: the abort caller and the faulting instruction."""
    addresses = set()
    for line in text.splitlines():
        if 'abort() was called at PC' in line or 'Guru Meditation' in line or 'epc1' in line:
            addresses.update(address for address in ADDRESS.findall(line) if is_code(address))
    return addresses


def panic_reason(text):
    """'abort() was called at PC <address>' or the Guru Meditation line of the decode output."""
    for line in text.splitlines():
        match = re.search(r'abort\(\) was called at PC (0x[0-9a-fA-F]{8})', line)
        if match:
            return 'abort() was called at PC %s' % match.group(1)
        if 'Guru Meditation' in line:
            return line.strip()
    return None


def stack_addresses(text):
    """Addresses gdb already resolved in the backtrace, no second lookup needed."""
    return set(re.findall(r'^#\d+\s+(0x[0-9a-fA-F]{8})', text, re.MULTILINE))


def symbolize(bin_dir, elf, addresses):
    """address -> '<function> at <file:line>' using the ELF of the crashed build."""
    addresses = sorted(addresses)
    addr2line = find_tool(bin_dir, '-addr2line')
    if not addresses or not addr2line:
        return {}
    result = subprocess.run([addr2line, '-f', '-C', '-e', elf] + addresses,
                            capture_output=True, text=True, errors='replace')
    lines = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    symbols = {}
    for index, address in enumerate(addresses):
        function = lines[2 * index] if 2 * index < len(lines) else '?'
        location = lines[2 * index + 1] if 2 * index + 1 < len(lines) else '?'
        symbols[address] = '%s at %s' % (function, location)
    return symbols


def python_with_esp_coredump():
    """Interpreter that can run `-m esp_coredump`: the PlatformIO penv or the current one, else None."""
    core = os.environ.get('PLATFORMIO_CORE_DIR') or os.path.join(os.path.expanduser('~'), '.platformio')
    for candidate in (os.path.join(core, 'penv', 'Scripts', 'python.exe'),
                      os.path.join(core, 'penv', 'bin', 'python'),
                      sys.executable):
        if os.path.isfile(candidate) and not subprocess.run(
                [candidate, '-c', 'import esp_coredump'], capture_output=True).returncode:
            return candidate
    return None


def decode(bin_dir, core, elf, chip):
    """Run esp-coredump on the core dump with the ELF of the crashed build."""
    gdb = find_tool(bin_dir, '-gdb')
    python = python_with_esp_coredump()
    if not python:
        # a report without the decode output looks like a decoded dump - fail loudly instead
        return ['esp_coredump'], (
            'ERROR: no Python with the esp_coredump module found. Run this script with the PlatformIO '
            'penv Python (e.g. %s) or install the module.\n' %
            os.path.join(os.path.expanduser('~'), '.platformio', 'penv', 'Scripts', 'python.exe'))
    command = [python, '-m', 'esp_coredump', '--chip', chip, 'info_corefile']
    if gdb:
        command += ['--gdb', gdb]
    command += ['-t', 'elf', '-c', core, elf]
    result = subprocess.run(command, capture_output=True, text=True, errors='replace')
    return command, result.stdout + result.stderr


def crash_summary(host, user, password):
    """Crash summary of the device as dict, None if there is no core dump."""
    status, body = fetch(host, '/savecrash.json', 'SID=%s' % session_id(user, password))
    return json.loads(body.decode('utf-8', 'replace')).get('coredump')


def erase(host, user, password):
    status, body = fetch(host, '/savecrash.json', 'cmd=erase-coredump&SID=%s' % session_id(user, password))
    return status, body.decode('utf-8', 'replace').strip()


def trace(args):
    """Download (or read) the dump, decode it with the matching ELF and resolve the panic addresses."""
    outdir = args.outdir or os.path.join(project_directory(), 'logs')
    os.makedirs(outdir, exist_ok=True)
    archive, elf, coredump = args.archive, args.elf, None

    if args.dump:
        with open(args.dump, 'rb') as file:
            raw = file.read()
    else:
        coredump = crash_summary(args.host, args.user, args.password)
        if not coredump:
            print('no core dump on %s' % args.host)
            return 1
        _, raw = fetch(args.host, '/coredump.bin', 'SID=%s' % session_id(args.user, args.password))
        if not archive:
            archive = find_archive(os.path.join(project_directory(), 'elf'), coredump['sha256'])

    if not archive and not elf:
        if args.dump:
            print('ERROR: a stored dump does not carry its firmware, pass the archive or elf of that build')
            print('       --archive elf/<env>_<build>.tar.gz (see elf/archive.log for the build numbers)')
        else:
            print('ERROR: no archive in elf/ matches the dump (firmware %s)' % coredump['sha256'])
            print('       only "pio run -t buildarchive" stores an archive, pass --archive or --elf')
        return 1

    names = ARCHIVE_NAME.match(os.path.basename(archive)) if archive else None
    env_name = names.group('env') if names else 'unknown'
    build = names.group('build') if names else (coredump['sha256'][:8] if coredump else 'dump')
    basename = '%s_%s' % (env_name, build)

    offset, core_elf = strip_flash_header(raw)
    if elf and coredump:
        with open(elf, 'rb') as file:
            if hash_prefix(file) != coredump['sha256']:
                print('WARNING: %s does not belong to the dump (%s)' % (elf, coredump['sha256']))

    core_bin = os.path.join(outdir, 'coredump_%s.bin' % basename)
    core_path = os.path.join(outdir, 'coredump_%s.elf' % basename)
    report_path = os.path.join(outdir, 'crash_%s.txt' % basename)
    with open(core_bin, 'wb') as file:
        file.write(raw)
    with open(core_path, 'wb') as file:
        file.write(core_elf)
    if coredump:
        with open(os.path.join(outdir, 'crash_%s.json' % basename), 'wt', encoding='utf-8',
                  newline='\n') as file:
            file.write(json.dumps(coredump, indent=2) + '\n')

    bin_dir = toolchain_directory()
    if not bin_dir:
        print('ERROR: no PlatformIO xtensa toolchain found (set PLATFORMIO_CORE_DIR)')
        return 1

    symbols = {}
    temporary = tempfile.mkdtemp(prefix='kfc_elf_')
    try:
        if not elf:
            with tarfile.open(archive) as tar:
                tar.extract('firmware.elf', temporary)
            elf = os.path.join(temporary, 'firmware.elf')
        command, output = decode(bin_dir, core_path, elf, args.chip)
        if 'CORE DUMP START' not in output:
            print('WARNING: esp-coredump produced no stack trace, the report is incomplete (%s)' %
                  report_path, file=sys.stderr)
        addresses = set(coredump['backtrace']) | {coredump['pc']} if coredump else set()
        addresses.update(panic_addresses(output))
        symbols = symbolize(bin_dir, elf, addresses - stack_addresses(output))

        with open(report_path, 'wt', encoding='utf-8', newline='\n') as file:
            file.write('# %s\n' % ' '.join(command))
            file.write('# core dump %s (flash header %u bytes), ELF %s\n' %
                       (os.path.basename(core_bin), offset, os.path.basename(elf)))
            if coredump:
                file.write('# %s\n\n' % json.dumps(coredump))
            file.write(output)
            file.write('\n==================== RESOLVED ADDRESSES ====================\n')
            for address in sorted(symbols):
                file.write('%s  %s\n' % (address, symbols[address]))
    finally:
        shutil.rmtree(temporary, ignore_errors=True)

    print('firmware: %s build %s (%s)' % (env_name, build, archive or elf))
    if coredump:
        print('crash: task %s, %s (%s), vaddr %s, backtrace corrupted %s' % (
            coredump['task'], coredump['cause_name'], coredump['cause'], coredump['vaddr'],
            coredump['corrupted']))
    reason = panic_reason(output)
    if reason:
        print('reason: %s' % reason)
    for address in sorted(symbols):
        print('%s  %s' % (address, symbols[address]))
    print('files: %s, %s, %s' % (core_bin, core_path, report_path))

    if args.erase:
        status, body = erase(args.host, args.user, args.password)
        print('erase: %s %s' % (status, body))
    return 0


def main():
    parser = argparse.ArgumentParser(description='KFC firmware core dump: summary, download, trace, erase')
    parser.add_argument('action', choices=['info', 'download', 'trace', 'erase'], help='what to do')
    parser.add_argument('--host', help='device address, not needed with --dump')
    parser.add_argument('-u', '--user', help='WebUI user name = device name (KFC0A4271 style)')
    parser.add_argument('-p', '--pw', '--password', dest='password', help='WebUI password')
    parser.add_argument('-o', '--output', help='file for the download action (default coredump.bin)')
    parser.add_argument('--dump', help='trace a core dump that was downloaded before (skips the download)')
    parser.add_argument('--outdir', help='directory for the trace artifacts (default logs/)')
    parser.add_argument('--archive', help='archive to take the firmware.elf from (default: match the dump)')
    parser.add_argument('--elf', help='firmware.elf of the crashed build instead of an archive')
    parser.add_argument('--chip', default='esp32', help='target chip for esp-coredump (default esp32)')
    parser.add_argument('--erase', action='store_true', help='erase the core dump after a successful trace')
    args = parser.parse_args()

    if not args.dump and not (args.host and args.user and args.password):
        parser.error('--host, --user and --password are required, or pass --dump for a stored one')

    try:
        if args.action == 'info':
            status, body = fetch(args.host, '/savecrash.json', 'SID=%s' % session_id(args.user, args.password))
            print(status, body.decode('utf-8', 'replace'))
        elif args.action == 'download':
            status, body = fetch(args.host, '/coredump.bin', 'SID=%s' % session_id(args.user, args.password))
            output = args.output or 'coredump.bin'
            with open(output, 'wb') as file:
                file.write(body)
            print('%s %u bytes -> %s (md5 %s)' % (status, len(body), output, hashlib.md5(body).hexdigest()))
        elif args.action == 'erase':
            status, body = erase(args.host, args.user, args.password)
            print(status, body)
        else:
            return trace(args)
    except urllib.error.HTTPError as error:
        print('HTTP %s: %s' % (error.code, error.read().decode('utf-8', 'replace')))
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

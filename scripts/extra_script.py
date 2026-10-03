import argparse
import io
import time
import sys
import re
import fnmatch
import hashlib
import tarfile
import shlex
import json
import click
import socket
from os import path
import os.path
import subprocess
from SCons.Script import ARGUMENTS
from pprint import pprint
import datetime
import platform
Import("env", "projenv")
try:
    import configparser
except ImportError:
    import ConfigParser as configparser

# verify that ESP32 or ESP8266 is set and equals 1
esp32 = False
esp8266 = False
defines = env.get('CPPDEFINES');
for define in defines:
    if isinstance(define, tuple):
        (key, val) = define
    else:
        key = define
        val = 1;
    if val==1:
        if key == 'ESP32':
            esp32 = True
        if key == 'ESP8266':
            esp8266 = True
if esp32 and esp8266:
    click.secho('ESP32 and ESP866 set to 1', fg='red')
    exit(1)
elif not esp32 and not esp8266:
    click.secho('Neither ESP32 nor ESP866 set to 1', fg='red')
    exit(1)

if esp32:
    click.secho('ESP32 detected', fg='green')
elif esp8266:
    click.secho('ESP866 detected', fg='green')

# add extra dirs for modules
sys.path.insert(0, path.abspath(path.join(env.subst("$PROJECT_DIR"), 'scripts', 'libs')))

verbose_flag = int(ARGUMENTS.get("PIOVERBOSE", 0))

flags = " ".join(env["LINKFLAGS"])
flags = flags.replace("-u _scanf_float", "")
newflags = flags.split()

env.Replace(LINKFLAGS=newflags)

# import subprocess
# result = subprocess.run(['git', 'describe'], stdout=subprocess.PIPE)
# git_version = result.stdout.decode().strip(' \t\r\n')

def verbose(msg, color=None):
    if not verbose_flag:
        return
    if color:
        click.secho(msg, fg=color)
    else:
        click.echo(msg)

def run_build_number(env, args):
    # see scripts/build_number.py, the counter is incremented before compiling and the number is
    # given back if the build never reaches the link step
    # NOTE: os.path, the module level `path` is shadowed by the PATH environment variable further down
    project_dir = env.subst('$PROJECT_DIR')
    scripts_dir = os.path.join(project_dir, 'scripts')
    sys.path.insert(0, scripts_dir)
    try:
        import build_number
        build_number.main(['--project-dir', project_dir] + list(args))
    finally:
        sys.path.remove(scripts_dir)


def new_build(source, target, env):
    # -t newbuild: increment without a build that has to confirm the number
    run_build_number(env, ['--env', env.subst('$PIOENV'), '--force', '--verbose'])


def commit_build_number(source, target, env):
    # the firmware was linked, the number stays
    run_build_number(env, ['--commit'])


# -------------------------------------------------------------------------
# build archive: core dump, elf, filesystem, sources and a log
#
# one compressed file per environment and build number:
#   elf/<env>_<build>.tar.gz = firmware.elf + firmware.bin + filesystem.bin
#                            + source/ + info.txt, see docs in info.txt
# -------------------------------------------------------------------------

ARCHIVE_SOURCE_ITEMS = ['src', 'include', 'conf', 'scripts', 'platformio.ini', 'KFCWebBuilder.json']
# lib/* is on github and committed, only the local KFCLibrary is added
ARCHIVE_SOURCE_LIBS = ['lib/KFCLibrary']
ARCHIVE_IGNORE_DIRS = {'.git', '.pio', '__pycache__', '.venv'}
ARCHIVE_IGNORE_FILES = ('.pyc', '.o', '.d')
# <environment>_<build>.tar.gz
ARCHIVE_NAME = re.compile(r'^(?P<env>.+)_(?P<build>\d+)\.tar\.gz$')
# esptool talks to the board at 115200 by default, a 1 MB filesystem read takes minutes that way
ESPTOOL_FAST_BAUD = '460800'


def esptool(env, args):
    """Run esptool, fast baud first, esptool's default speed if the adapter does not support it."""
    port = env.subst('$UPLOAD_PORT')
    speed = env.subst('$UPLOAD_SPEED')
    speeds = [speed] if speed else [ESPTOOL_FAST_BAUD, None]
    for baud in speeds:
        command = [env.subst('$PYTHONEXE'), env.subst('$UPLOADER'), '--port', port]
        if baud:
            command += ['-b', baud]
        command += list(args)
        verbose('esptool %s' % ' '.join(command))
        if subprocess.run(command).returncode == 0:
            return True
        if baud:
            click.secho('WARNING: %s baud failed, retrying with the default speed' % baud, fg='yellow')
    return False


def erase_core_dump(target, source, env):
    # the ESP32 panic handler stores the crash report in the `coredump` partition, an old dump would
    # be mixed up with the next crash of the new firmware. Done over the serial port, no WebUI and
    # no credentials are needed and it works if the device has crashed
    if not esp32:
        return
    port = env.subst('$UPLOAD_PORT')
    uploader = env.subst('$UPLOADER')
    if not port or '@' in port:
        click.secho('ERROR: no serial port at this environment, cannot erase the ESP32 core dump', fg='red')
        env.Exit(1)
    if not uploader or not os.path.isfile(uploader):
        click.secho('ERROR: uploader "%s" not found, cannot erase the ESP32 core dump' % uploader, fg='red')
        env.Exit(1)
    partition = read_partition(env.subst('$PARTITIONS_TABLE_CSV'), 'coredump')
    if not partition:
        click.secho('WARNING: no coredump partition, nothing to erase', fg='yellow')
        return
    offset, size = partition
    verbose('Erasing the core dump partition (0x%x, %u bytes) on %s' % (offset, size, port))
    if not esptool(env, ['erase_region', hex(offset), hex(size)]):
        click.secho('ERROR: erasing the core dump failed, is the port in use?', fg='red')
        env.Exit(1)


def read_build_number(env):
    project_dir = env.subst('$PROJECT_DIR')
    scripts_dir = os.path.join(project_dir, 'scripts')
    sys.path.insert(0, scripts_dir)
    try:
        import build_number
        return build_number.read_counter(os.path.join(project_dir, build_number.DEFAULT_COUNTER))
    finally:
        sys.path.remove(scripts_dir)


def file_digest(filename, algorithm):
    result = hashlib.new(algorithm)
    with open(filename, 'rb') as file:
        for block in iter(lambda: file.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def parse_partition_size(value):
    value = value.strip().lower()
    if value.startswith('0x'):
        return int(value, 16)
    for suffix, factor in (('k', 1024), ('m', 1024 * 1024)):
        if value.endswith(suffix):
            return int(value[:-1]) * factor
    return int(value)


def read_partition(filename, name):
    """(offset, size) of a partition of a partition table csv or None."""
    if not filename or not os.path.isfile(filename):
        return None
    try:
        with open(filename, 'rt', encoding='utf-8', errors='replace') as file:
            for line in file:
                line = line.split('#')[0].strip()
                if not line:
                    continue
                fields = [item.strip() for item in line.split(',')]
                if len(fields) >= 5 and fields[0] == name:
                    return parse_partition_size(fields[3]), parse_partition_size(fields[4])
    except Exception as e:
        click.secho('WARNING: cannot read %s: %s' % (filename, e), fg='yellow')
    return None


def add_sources(archive, project_dir, items):
    """Add the source items as source/<item> straight from the working tree, no copy on disk."""
    added = []
    for item in items:
        src = os.path.join(project_dir, item.replace('/', os.sep))
        if not os.path.exists(src):
            click.secho('WARNING: source "%s" does not exist' % item, fg='yellow')
            continue
        if os.path.isfile(src):
            archive.add(src, arcname='source/' + item)
        else:
            for root, dirs, files in os.walk(src):
                dirs[:] = sorted(name for name in dirs if name not in ARCHIVE_IGNORE_DIRS)
                for name in sorted(files):
                    if name.endswith(ARCHIVE_IGNORE_FILES):
                        continue
                    full = os.path.join(root, name)
                    relative = os.path.relpath(full, project_dir).replace(os.sep, '/')
                    archive.add(full, arcname='source/' + relative)
        added.append(item)
    return added


def add_text(archive, name, text):
    """Add a small text file to the archive without a temporary file on disk."""
    data = text.encode('utf-8')
    info = tarfile.TarInfo(name)
    info.size = len(data)
    info.mtime = int(time.time())
    archive.addfile(info, io.BytesIO(data))


def remove_superseded_archives(output_dir, env_name, keep):
    """Delete the other archives of this environment: only the newest one is kept.

    Test uploads would otherwise pile up one ~55 MB archive per try, and only the firmware that is
    flashed last is the one a crash has to be decoded against.
    """
    removed = []
    for name in sorted(os.listdir(output_dir)):
        if name == keep:
            continue
        match = ARCHIVE_NAME.match(name)
        if match and match.group('env') == env_name:
            os.remove(os.path.join(output_dir, name))
            removed.append(name)
    return removed


def read_filesystem(env, output):
    """Byte exact copy of the filesystem partition read from the device."""
    port = env.subst('$UPLOAD_PORT')
    uploader = env.subst('$UPLOADER')
    if not port or '@' in port:
        click.secho('WARNING: no serial port at this environment, the filesystem is not copied', fg='yellow')
        return 'not available'
    if not uploader or not os.path.isfile(uploader):
        click.secho('WARNING: uploader "%s" not found, the filesystem is not copied' % uploader, fg='yellow')
        return 'not available'
    partitions_csv = env.subst('$PARTITIONS_TABLE_CSV')
    if not partitions_csv:
        partitions_csv = os.path.join(env.subst('$PROJECT_DIR'), env.GetProjectOption('board_build.partitions', ''))
    partition = read_partition(partitions_csv, 'spiffs')
    if not partition:
        click.secho('WARNING: no spiffs partition in "%s", the filesystem is not copied' % partitions_csv, fg='yellow')
        return 'not available'
    offset, size = partition
    verbose('Reading the filesystem (0x%x, %u bytes) from %s' % (offset, size, port))
    if not esptool(env, ['read_flash', hex(offset), hex(size), output]) or not os.path.isfile(output):
        click.secho('WARNING: reading the filesystem failed, is the port in use?', fg='yellow')
        return 'not available'
    return '%s (%u bytes, md5 %s)' % (os.path.basename(output), os.path.getsize(output), file_digest(output, 'md5'))


def git_revision(directory, excludes=()):
    """'<short commit>' or '<short commit> (dirty)' of a git repository, None if it is not one."""
    def run(args):
        try:
            result = subprocess.run(['git', '-C', directory] + args, capture_output=True, text=True)
        except Exception:
            return None
        return result.stdout.strip() if result.returncode == 0 else None

    if not os.path.isdir(os.path.join(directory, '.git')):
        return None
    commit = run(['rev-parse', 'HEAD'])
    if not commit:
        return None
    args = ['status', '--porcelain', '--untracked-files=no']
    if excludes:
        args += ['--', '.'] + [':(exclude)%s' % item for item in excludes]
    return '%s%s' % (commit[:12], ' (dirty)' if run(args) else '')


def archive_build(target, source, env):
    # after a successful upload: ELF, bin, the filesystem read from the device, the sources and a log.
    # Everything goes straight into the tar.gz, there is no temporary copy of the build (the ELF alone
    # is ~90 MB) and nothing is left behind when this step is interrupted
    project_dir = env.subst('$PROJECT_DIR')
    env_name = env.subst('$PIOENV')
    build_dir = env.subst('$BUILD_DIR')
    elf = os.path.join(build_dir, 'firmware.elf')
    image = os.path.join(build_dir, 'firmware.bin')
    if not os.path.isfile(elf):
        click.secho('ERROR: %s not found, nothing is archived' % elf, fg='red')
        return

    number, date, _ = read_build_number(env)
    output_dir = os.path.join(project_dir, 'elf')
    os.makedirs(output_dir, exist_ok=True)
    name = '%s_%d.tar.gz' % (env_name, number)
    output = os.path.join(output_dir, name)
    # the read filesystem and the growing tar.gz live in the build directory and only a finished
    # archive is moved into elf/: an interrupted archive leaves no broken .tar.gz there
    filesystem_bin = os.path.join(build_dir, 'filesystem.bin')
    temporary = os.path.join(build_dir, '.archive.tar.gz')
    addr2line = 'xtensa-esp32-elf-addr2line' if esp32 else 'xtensa-lx106-elf-addr2line'

    started = time.time()
    filesystem = read_filesystem(env, filesystem_bin)
    sources = [item for item in ARCHIVE_SOURCE_ITEMS + ARCHIVE_SOURCE_LIBS
               if os.path.exists(os.path.join(project_dir, item.replace('/', os.sep)))]
    info = [
        '; build archive of %s' % env_name,
        ';',
        '; decode an address of a stack trace, crash log or exception dump:',
        ';   %s -f -C -i -e firmware.elf 0x400d1234' % addr2line,
        '; ESP32 core dump of the device (summary, download, erase):',
        ';   python scripts/tools/kfc_coredump.py info|download|erase --host <ip> -u <device> -p <password>',
        ';   the download starts with a 20 byte flash header, strip it and decode:',
        ';   esp-coredump --chip esp32 info_corefile --gdb <xtensa-esp32-elf-gdb> -t elf -c <dump> firmware.elf',
        ';',
        '; source of this build: project + lib/KFCLibrary, restore with the revisions below,',
        '; "(dirty)" means the tree had uncommitted changes',
        ';',
        'date = %s' % datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
        'build = %d' % number,
        'build_date = %s' % date,
        'env = %s' % env_name,
        'platform = %s' % env.subst('$PIOPLATFORM'),
        'board = %s' % env.subst('$BOARD'),
        'elf_md5 = %s' % file_digest(elf, 'md5'),
        'elf_sha256 = %s' % file_digest(elf, 'sha256'),
        'filesystem = %s' % filesystem,
        'git = %s' % (git_revision(project_dir, ('include/build_number.txt', 'src/build_number.cpp')) or 'unavailable'),
        'lib/KFCLibrary = %s' % (git_revision(os.path.join(project_dir, 'lib', 'KFCLibrary')) or 'unavailable'),
        'sources = %s' % ', '.join(sources),
    ]
    try:
        # compresslevel 6: measured on the real 132 MB content, level 6 is 52.8 MB in 4.7 s and
        # level 8 only saves 0.2 MB in twice the time (level 9 is slower than level 8 for ~1 MB)
        with tarfile.open(temporary, 'w:gz', compresslevel=6) as archive:
            archive.add(elf, arcname='firmware.elf')
            if os.path.isfile(image):
                archive.add(image, arcname='firmware.bin')
            if os.path.isfile(filesystem_bin):
                archive.add(filesystem_bin, arcname='filesystem.bin')
            add_sources(archive, project_dir, sources)
            add_text(archive, 'info.txt', '\n'.join(info) + '\n')
        os.replace(temporary, output)
    except Exception as exception:
        # the upload itself succeeded, an archive problem must not turn that into a failed command
        click.secho('WARNING: archiving the build failed: %s' % exception, fg='yellow')
        return
    finally:
        for leftover in (filesystem_bin, temporary):
            if os.path.isfile(leftover):
                os.remove(leftover)

    click.secho('Archived %s (%u bytes, %u s)' % (name, os.path.getsize(output), time.time() - started), fg='green')
    # a test upload is only archived until the next one: the previous archive of this environment goes away
    removed = remove_superseded_archives(output_dir, env_name, name)
    with open(os.path.join(output_dir, 'archive.log'), 'at', encoding='utf-8', newline='\n') as file:
        file.write('%s | build %d | %s | %s | %u bytes | %u s | filesystem: %s\n' % (
            datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), number, env_name, name,
            os.path.getsize(output), time.time() - started, filesystem))
        for old in removed:
            file.write('%s | build %d | %s | %s | removed, superseded by %s\n' % (
                datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), number, env_name, old, name))
    if removed:
        click.secho('Removed %s, only the newest archive of %s is kept' % (', '.join(removed), env_name), fg='yellow')


def modify_upload_command(source, target, env, fs=False):

    upload_command = env.GetProjectOption('custom_upload_command', '')
    if not upload_command:
        click.secho('custom_upload_command is not defined', fg='yellow')
        return
    if env['UPLOAD_PROTOCOL'] != 'espota':
        click.echo('protocol is not espota')
        return

    upload_port = env.subst(env.GetProjectOption('upload_port'))
    m = re.match(r'(?P<username>[^:]+):(?P<password>[^@]+)@(?P<hostname>.+)', upload_port)
    if not m:
        click.echo('upload_port must be <username>:<password>@<hostname>')
        aota = 'http://%s/start-arduino-ota' % upload_port
        click.echo('running "curl -s %s"' % aota)
        return_code = subprocess.run(['curl', '-s', aota], shell=(platform.system() == 'Windows')).returncode
        print();
        return
    device = m.groupdict()

    args = ['--user', device['username'], '--pass',
            device['password'], '--image', str(source[0])]
    if fs:
        args.append('uploadfs')
        args.append(device['hostname'])
    else:
        args.append('upload')
        args.append(device['hostname'])

    env.Replace(UPLOAD_FLAGS=' '.join(args), UPLOAD_COMMAND=upload_command, UPLOADCMD=upload_command)

def modify_upload_command_fs(source, target, env):
    modify_upload_command(source, target, env, True)


# def git_get_head():
#     p = subprocess.Popen(["%GITEXE%", "rev-parse", "HEAD"], stdout=subprocess.PIPE, text=True)
#     output, errors = p.communicate()
#     if p.wait()==0:
#         return output.strip()
#     return "NA"

def firmware_config(source, target, env, action):

    if env["UPLOAD_PROTOCOL"] != 'espota':
        click.secho('UPLOAD_PROTOCOL not espota', fg='yellow')
        env.Exit(1)

    try:
        m = re.match(r'(?P<username>[^:]+):((?P<hash>[a-f0-9]{80})|(?P<password>[^@]+))@(?P<hostname>.*)', env.subst(
            env.GetProjectOption('upload_port')))
        device = m.groupdict()
        if not device['username'].startswith("KFC"):
            raise RuntimeError("invalid username: %s" % device['username'])
        if device['hash'] == None and len(device['password']) < 6:
            raise RuntimeError("invalid password: ...")
        address = socket.gethostbyname(device['hostname'])
        device['address'] = address
    except Exception as e:
        click.echo('%s' % e)
        click.secho('requires "upload_port = <username>:<password/hash>@<hostname>" at the environment', fg='yellow')
        env.Exit(1)

    if device['hash'] != None:
        verbose("using hash as authentication")
    else:
        session = kfcfw.Session()
        device['hash'] = session.generate(
            device['username'], device['password'])
        verbose("generating hash from password for the authentication")

    device['name'] = '%s:***@%s' % (device['username'], device['hostname'])

    parser = argparse.ArgumentParser(description="")
    parser.add_argument("action", help="action to execute", choices=["factory", "alive", "autodiscovery"])
    args = parser.parse_args(shlex.split(env.subst("$UPLOAD_PORT")))

    click.echo('Connecting to %s...' % device['name'])

    payload_sent = False
    timeout = time.monotonic() + 30
    sock = kfcfw.OTASerialConsole(device['hostname'], device['hash'])

    if action == 'factory':
        while time.monotonic() < timeout and not sock.is_closed:
            if sock.is_authenticated and payload_sent == False:
                sock.ws.send('+factory\r\n')
                sock.ws.send('+store\r\n')
                sock.ws.send('+rst\r\n')
                payload_sent = True
                timeout = time.monotonic() + 3
            time.sleep(1)

    elif args.action == 'auto_discovery':
        while time.monotonic() < timeout and not sock.is_closed:
            if sock.is_authenticated and payload_sent == False:
                sock.ws.send('+mqtt=auto\r\n')
                payload_sent = True
                timeout = time.monotonic() + 3
            time.sleep(1)

    sock.close()

def upload_file(source, target, env):

    src = path.abspath(env.subst(env.GetProjectOption('custom_upload_file_src', '')))
    dst = env.subst(env.GetProjectOption('custom_upload_file_dst', ''))

    if not src:
        click.secho('custom_upload_file_src missing', fg='yellow')
        env.Exit(1)
    if not os.path.exists(src):
        click.secho('custom_upload_file_src does not exist: %s' % src, fg='yellow')
        env.Exit(1)
    if not dst:
        click.secho('custom_upload_file_dst missing', fg='yellow')
        env.Exit(1)


    script = path.abspath(env.subst('$PROJECT_DIR/scripts/tools/kfcfw_ota.py'))

    if env["UPLOAD_PROTOCOL"] != 'espota':
        click.secho('UPLOAD_PROTOCOL not espota', fg='yellow')
        env.Exit(1)

    auth = env.GetProjectOption('custom_upload_file_auth')
    if not auth:
        click.secho('custom_upload_file_auth missing', fg='yellow')
        env.Exit(1)

    try:
        m = re.match(r'(?P<username>[^:]+):(?P<password>[^@]+)@(?P<hostname>.+)', auth)
        if not m:
            click.echo('custom_upload_file_auth must be <username>:<password>@<hostname>')
        device = m.groupdict()
        if not device['username'].startswith("KFC"):
            raise RuntimeError("invalid username: %s" % device['username'])
        if len(device['password']) < 6:
            raise RuntimeError("invalid password: ...")
        address = socket.gethostbyname(device['hostname'])
        device['address'] = address
    except Exception as e:
        click.echo('Exception: %s' % e)
        click.secho('requires "custom_upload_file_auth = <username>:<password>@<hostname>" at the environment', fg='yellow')
        env.Exit(1)

    device['name'] = '%s:***@%s' % (device['username'], device['hostname'])

    args = [ env.subst('$PYTHONEXE'), script, 'uploadfile', device['address'], '-u', device['username'], '-p', device['password'], '-I', src, '--fs-dst', dst ]

    verbose('Uploading to %s: %s: %s' % (device['name'], src, dst))

    return_code = subprocess.run(args, shell=(platform.system() == 'Windows')).returncode
    if return_code != 0:
        click.secho('failed to run: %s' % ' '.join(args))
        env.Exit(1)

    if dst.endswith('.hex'):
        click.secho('Flash Firmware with command: +STK500V1F=%s' % dst, fg='yellow')
    else:
        click.secho('Uploaded %s' % dst, fg='yellow')

# def create_patch_file(source, target, env):
#     packages_dir = path.abspath(env.subst('$PROJECT_PACKAGES_DIR'))
#     new_dir = path.join(packages_dir, 'framework-arduinoespressif8266')
#     orig_dir = path.join(packages_dir, 'framework-arduinoespressif8266_orig')
#     with open(path.join(orig_dir, 'package.json'), "rt") as f:
#         info = json.loads(f.read())
#     target = path.abspath(env.subst('$PROJECT_DIR/patches/%s%s.patch' % (info['name'], info['version'])))

#     diff_bin = 'c:/cygwin64/bin/diff'

#     packages_dir = packages_dir.replace('\\', '/')
#     orig_dir = orig_dir.replace('\\', '/')
#     new_dir = new_dir.replace('\\', '/')

#     orig_dir = '.' + orig_dir[len(packages_dir):]
#     new_dir = '.' + new_dir[len(packages_dir):]

#     args = [diff_bin, '-r', '-Z', '-P4', orig_dir, new_dir, '>', target]

#     wd = os.getcwd()
#     try:
#         os.chdir(packages_dir)
#         return_code = subprocess.run(args, shell=True).returncode
#     finally:
#         os.chdir(wd)

#     click.secho('Output file: %s' % target, fg='green')

def dump_info(source, target, env):
    print(source[0].get_abspath())
    # print(target)

# ESP32
# change MKSPIFFSTOOL for ESP32 to mklittlefs
if esp32 and env.GetProjectOption('board_build.filesystem') == 'littlefs':
    click.echo('board_build.filesystem = littlefs: ', nl=False)
    click.secho('replacing MKSPIFFS with MKLITTLEFS', fg='yellow')
    environ = env.get('ENV')
    path = environ.get('PATH')
    path = path.replace('tool-mkspiffs', 'tool-mklittlefs')
    environ['PATH'] = path
    env.Replace(MKSPIFFSTOOL='mklittlefs', ENV=environ, ESP32_SPIFFS_IMAGE_NAME='littlefs')

env.AddPreAction('upload', modify_upload_command)
env.AddPreAction('uploadota', modify_upload_command)
env.AddPreAction('uploadfs', modify_upload_command_fs)
env.AddPreAction('uploadfsota', modify_upload_command_fs)

# erase the ESP32 core dump before flashing and archive the build after a successful upload
env.AddPreAction('upload', erase_core_dump)
env.AddPreAction('uploadota', erase_core_dump)
env.AddPostAction('upload', archive_build)
env.AddPostAction('uploadota', archive_build)

# env.AddPreAction(env['PIOMAINPROG'], dump_info)

env.AlwaysBuild(env.Alias('newbuild', None, new_build))

env.AddPostAction(env['PIOMAINPROG'], commit_build_number)

# env.AlwaysBuild(env.Alias('patch_file', None, create_patch_file))
# env.AlwaysBuild(env.Alias('patch-file', None, create_patch_file))

env.AlwaysBuild(env.Alias('kfcfw_factory', None, lambda source, target, env: firmware_config(source, target, env, 'factory')))
env.AlwaysBuild(env.Alias('kfcfw_auto_discovery', None, lambda source, target, env: firmware_config(source, target, env, 'auto_discovery')))
env.AlwaysBuild(env.Alias('upload_file', None, lambda source, target, env: upload_file(source, target, env)))

env.AddCustomTarget('kfcfw_factory', None, [], title='factory reset', description='KFC firmware OTA factory reset', always_build=False)
env.AddCustomTarget('kfcfw_auto_discovery', None, [], title='auto discovery', description='KFC firmware OTA publish auto discovery', always_build=False)
env.AddCustomTarget('upload_file', None, [], title='upload file', description='Upload file to file system', always_build=False)

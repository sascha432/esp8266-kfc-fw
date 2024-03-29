import argparse
import time
import sys
import re
import fnmatch
import shlex
import json
import click
import socket
from os import path
import os.path
import shutil
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

def new_build(source, target, env):
    args = [ env.subst('$PYTHONEXE'), env.subst('$PROJECT_DIR/scripts/build_number.py'), '-v', env.subst('$PROJECT_DIR/include/build.h.current') ]
    p = subprocess.Popen(args, text=True)
    p.wait()


def modify_upload_command(source, target, env, fs=False):

    if fs == False:
        new_build(source, target, env)

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

    args.append('--elf')
    args.append(path.abspath(path.join(env.subst("$PROJECT_DIR"), 'elf')))
    args.append('--ini')
    args.append(env.subst("$PROJECT_DIR"))

    env.Replace(UPLOAD_FLAGS=' '.join(args), UPLOAD_COMMAND=upload_command, UPLOADCMD=upload_command)

def modify_upload_command_fs(source, target, env):
    modify_upload_command(source, target, env, True)


# def git_get_head():
#     p = subprocess.Popen(["%GITEXE%", "rev-parse", "HEAD"], stdout=subprocess.PIPE, text=True)
#     output, errors = p.communicate()
#     if p.wait()==0:
#         return output.strip()
#     return "NA"

def which(name, env, flags=os.F_OK):
    result = []
    extensions = env["ENV"]["PATHEXT"].split(os.pathsep)
    if not extensions:
        extensions = [".exe", ""]

    for path in env["ENV"]["PATH"].split(os.pathsep):
        path = os.path.join(path, name)
        if os.access(path, flags):
            result.append(os.path.normpath(path))
        for ext in extensions:
            whole = path + ext
            if os.access(whole, os.X_OK):
                result.append(os.path.normpath(whole))
    return result


def mem_analyzer(source, target, env):

    return;

    if esp32==False and platform.system() == 'Windows':
        # https://github.com/Sermus/ESP8266_memory_analyzer
        args = [
            path.realpath(path.join(env.subst("$PROJECT_DIR"), "./scripts/tools/MemAnalyzer.exe")),
            which('xtensa-lx106-elf-objdump.exe', env)[0],
            '"%s"' % path.realpath(str(target[0]))
        ]
        p = subprocess.Popen(args, text=True)
        p.wait()


def disassemble(source, target, env):

    verbose = int(ARGUMENTS.get("PIOVERBOSE", 0))

    source = path.abspath(env.subst('$PIOMAINPROG'))
    target = env.subst(env.GetProjectOption('custom_disassemble_target', '$BUILD_DIR/${PROGNAME}.lst'))
    target = path.abspath(target)

    command = env.subst(env.GetProjectOption('custom_disassemble_bin', env['CC'].replace('gcc', 'objdump')))

    options = re.split(r'[\s]', env.subst(env.GetProjectOption('custom_disassemble_options', '-S -C')))

    args = [command] + options + [source, '>', target]
    if verbose:
        click.echo(' '.join(args))

    return_code = subprocess.run(args, shell=True).returncode
    if return_code != 0:
        click.secho('failed to run: %s' % ' '.join(args))
        env.Exit(1)

    click.echo('-' * click.get_terminal_size()[0])
    click.secho('Created: ', fg='yellow', nl=False)
    click.secho(target)

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

# env.AddPreAction(env['PIOMAINPROG'], dump_info)

env.AlwaysBuild(env.Alias('newbuild', None, new_build))

env.AddPostAction(env['PIOMAINPROG'], mem_analyzer)

# env.AlwaysBuild(env.Alias('patch_file', None, create_patch_file))
# env.AlwaysBuild(env.Alias('patch-file', None, create_patch_file))

env.AlwaysBuild(env.Alias('disasm', None, disassemble))
env.AlwaysBuild(env.Alias('disassemble', [env['PIOMAINPROG']], disassemble))

env.AlwaysBuild(env.Alias('kfcfw_factory', None, lambda source, target, env: firmware_config(source, target, env, 'factory')))
env.AlwaysBuild(env.Alias('kfcfw_auto_discovery', None, lambda source, target, env: firmware_config(source, target, env, 'auto_discovery')))
env.AlwaysBuild(env.Alias('upload_file', None, lambda source, target, env: upload_file(source, target, env)))

env.AddCustomTarget('disassemble', None, [], title='disassemble main prog', description='run objdump to create disassembly', always_build=True)
env.AddCustomTarget('kfcfw_factory', None, [], title='factory reset', description='KFC firmware OTA factory reset', always_build=False)
env.AddCustomTarget('kfcfw_auto_discovery', None, [], title='auto discovery', description='KFC firmware OTA publish auto discovery', always_build=False)
env.AddCustomTarget('upload_file', None, [], title='upload file', description='Upload file to file system', always_build=False)

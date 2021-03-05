Import("env", "projenv")
import datetime
from pprint import pprint
try:
    import configparser
except ImportError:
    import ConfigParser as configparser
from SCons.Script import ARGUMENTS
import subprocess
import shutil
import os.path
from os import path
import socket
import click
import json
import shlex
import fnmatch
import socket
import re
import sys
import time
import argparse
import click

sys.path.insert(0, path.abspath(path.join(env.subst("$PROJECT_DIR"), 'scripts', 'libs')))
import kfcfw

def new_build(source, target, env):
    env.Execute(env['PYTHONEXE'] + " \"${PROJECT_DIR}/scripts/build_number.py\" -v \"${PROJECT_DIR}/include/build.h\"");

def modify_upload_command(source, target, env, fs = False):
    if fs==False:
        new_build(source, target, env)

    upload_command = env.GetProjectOption('custom_upload_command', '');
    if not upload_command:
        click.secho('custom_upload_command is not defined', fg='yellow')
        return
    if env['UPLOAD_PROTOCOL']!='espota':
        click.echo('protocol is not espota')
        return

    m = re.match(r'(?P<username>[^:]+):(?P<password>[^@]+)@(?P<hostname>.+)', env.subst(env.GetProjectOption('upload_port')))
    if not m:
        click.echo('upload_port must be <username>:<password>@<hostname>')
        return
    device = m.groupdict()

    args = [ '--user', device['username'], '--pass', device['password'], '--image', str(source[0]) ]
    if fs:
        args.append('uploadfs')
        args.append(device['hostname'])
    else:
        args.append('upload')
        args.append(device['hostname'])

    args.append('--elf')
    args.append(os.path.abspath(os.path.join(env.subst("$PROJECT_DIR"), 'elf')))
    args.append('--ini')
    args.append(env.subst("$PROJECT_CONFIG"))

    env.Replace(UPLOAD_FLAGS=' '.join(args), UPLOAD_COMMAND=upload_command)


def modify_upload_command_fs(source, target, env):
    modify_upload_command(source, target, env, True)


def git_get_head():
    p = subprocess.Popen(["%GITEXE%", "rev-parse", "HEAD"], stdout=subprocess.PIPE, text=True)
    output, errors = p.communicate()
    if p.wait()==0:
        return output.strip()
    return "NA"

# def copy_file(source_path):
#     id = get_last_build_id()
#     if id!=None:
#         target_file = os.path.basename(source_path)
#         target_dir = "\\\\192.168.0.3\\kfcfw\\" + id + "_"
#         target_path = target_dir + target_file
#         shutil.copy2(source_path, target_path)
#     try:
#         with open(target_dir + "git_head", 'wt') as file:
#             file.write(git_get_head())
#     except:
#         pass

# def copy_firmware(source, target, env):
#     copy_file(str(target[0]))
#     # copy_file(str(target[0]).replace(".elf", ".bin"))
#     copy_file("platformio.ini")

# # remove all duplicates from CPPDEFINES
# # last define is left over
# dupes = {}
# values = {}
# n = 0
# cppdefines = env.get("CPPDEFINES", [])
# for item in cppdefines:
#     if isinstance(item, tuple):
#         key = item[0]
#         val = item[1]
#     else:
#         key = item
#         val = 1
#     if key in dupes:
#         dupes[key].append(n)
#         values[key].append(val)
#     else:
#         dupes[key] = [n]
#         values[key] = [val]
#     n += 1

# for key in dupes:
#     if len(dupes[key])>1:
#         # n = 0
#         # for index in dupes[key]:
#         #     print('key=%s num=%u/%u value=%s index=%u' % (key, n, len(dupes[key]), values[key][n], index))
#         #     print(dupes[key][0:-1], dupes[key])
#         #     n =+ 1

#         for index in dupes[key][0:-1]:
#             del cppdefines[index]

# env.Replace(CPPDEFINES=cppdefines);

def copy_spiffs(source, target, env):
    copy_file(str(target[0]))

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
    # https://github.com/Sermus/ESP8266_memory_analyzer
    args = [
        os.path.realpath(os.path.join(env.subst("$PROJECT_DIR"), "./scripts/tools/MemAnalyzer.exe")),
        which('xtensa-lx106-elf-objdump.exe', env)[0],
        os.path.realpath(str(target[0]))
    ]
    print(' '.join(args))
    p = subprocess.Popen(args, text=True)
    p.wait()

def disassemble(source, target, env):

    verbose = int(ARGUMENTS.get("PIOVERBOSE", 0))

    source = path.abspath(env.subst('$PIOMAINPROG'))
    target = env.subst(env.GetProjectOption('custom_disassemble_target', '$BUILD_DIR/${PROGNAME}.lst'))
    target = path.abspath(target)

    command = env.subst(env.GetProjectOption('custom_disassemble_bin', env['CC'].replace('gcc', 'objdump')))

    options = re.split(r'[\s]', env.subst(env.GetProjectOption('custom_disassemble_options', '-S -C')))

    args = [ command ] + options + [source, '>', target]
    if verbose:
        click.echo(' '.join(args))

    return_code = subprocess.run(args, shell=True).returncode
    if return_code!=0:
        click.secho('failed to run: %s' % ' '.join(args))
        env.Exit(1)

    click.echo('-' * click.get_terminal_size()[0])
    click.secho('Created: ', fg='yellow', nl=False)
    click.secho(target)

def firmware_config(source, target, env, action):

    verbose_flag = int(ARGUMENTS.get("PIOVERBOSE", 0))

    def verbose(msg, color=None):
        if not verbose_flag:
            return
        if color:
            click.secho(msg, fg=color)
        else:
            click.echo(msg)


    if env["UPLOAD_PROTOCOL"]!='espota':
        click.secho('UPLOAD_PROTOCOL not espota', fg='yellow')
        env.Exit(1)

    try:
        m = re.match(r'(?P<username>[^:]+):((?P<hash>[a-f0-9]{80})|(?P<password>[^@]+))@(?P<hostname>.*)', env.subst(env.GetProjectOption('upload_port')))
        device = m.groupdict();
        if not device['username'].startswith("KFC"):
            raise RuntimeError("invalid usename: %s" % device['username'])
        if device['hash']==None and len(device['password'])<6:
            raise RuntimeError("invalid password: ...")
        address = socket.gethostbyname(device['hostname'])
        device['address'] = address
    except Exception as e:
        click.echo('%s' % e)
        click.secho('requires "upload_port = <username>:<password/hash>@<hostame>" at the environment', fg='yellow')
        env.Exit(1)

    if device['hash']!=None:
        verbose("using hash as authentication")
    else:
        session = kfcfw.Session()
        device['hash'] = session.generate(device['username'], device['password'])
        verbose("generating hash from password for the authentication")

    device['name'] = '%s:***@%s' % (device['username'], device['hostname'])

    parser = argparse.ArgumentParser(description="")
    parser.add_argument("action", help="action to execute", choices=["factory", "alive", "autodiscovery"])
    args = parser.parse_args(shlex.split(env.subst("$UPLOAD_PORT")))

    click.echo('Connecting to %s...' % device['name'])

    payload_sent = False
    timeout = time.monotonic() + 30
    sock = kfcfw.OTASerialConsole(device['hostname'], device['hash'])

    if action=='factory':
        while time.monotonic()<timeout and not sock.is_closed:
            if sock.is_authenticated and payload_sent==False:
                sock.ws.send('+factory\r\n')
                sock.ws.send('+store\r\n')
                sock.ws.send('+rst\r\n')
                payload_sent = True
                timeout = time.monotonic() + 3
            time.sleep(1)

    elif args.action=='auto_discovery':
        while time.monotonic()<timeout and not sock.is_closed:
            if sock.is_authenticated and payload_sent==False:
                sock.ws.send('+mqtt=auto\r\n')
                payload_sent = True
                timeout = time.monotonic() + 3
            time.sleep(1)

    sock.close()

def create_patch_file(source, target, env):
    packages_dir = path.abspath(env.subst('$PROJECT_PACKAGES_DIR'))
    new_dir = path.join(packages_dir, 'framework-arduinoespressif8266')
    orig_dir = path.join(packages_dir, 'framework-arduinoespressif8266_orig')
    with open(path.join(orig_dir, 'package.json'), "rt") as f:
        info = json.loads(f.read())
    target = path.abspath(env.subst('$PROJECT_DIR/patches/%s%s.patch' % (info['name'], info['version'])))

    diff_bin = 'c:/cygwin64/bin/diff'

    packages_dir = packages_dir.replace('\\', '/');
    orig_dir = orig_dir.replace('\\', '/');
    new_dir = new_dir.replace('\\', '/');

    orig_dir = '.' + orig_dir[len(packages_dir):]
    new_dir = '.' + new_dir[len(packages_dir):]

    args = [ diff_bin, '-r', '-Z', '-P4', orig_dir, new_dir, '>', target]

    wd = os.getcwd()
    try:
        os.chdir(packages_dir)
        return_code = subprocess.run(args, shell=True).returncode
    finally:
        os.chdir(wd)

    click.secho('Output file: %s' % target, fg='green')

env.AddPreAction("upload", modify_upload_command)
env.AddPreAction("uploadota", modify_upload_command)
env.AddPreAction("uploadfs", modify_upload_command_fs)
env.AddPreAction("uploadfsota", modify_upload_command_fs)

env.AlwaysBuild(env.Alias("newbuild", None, new_build))

env.AddPostAction(env['PIOMAINPROG'], mem_analyzer)
# env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_firmware)

env.AlwaysBuild(env.Alias("patch_file", None, create_patch_file))

env.AlwaysBuild(env.Alias("disasm", None, disassemble))
env.AlwaysBuild(env.Alias("disassemble", [env['PIOMAINPROG']], disassemble))

env.AlwaysBuild(env.Alias("kfcfw_factory", None, lambda source, target, env: firmware_config(source, target, env, "factory")))
env.AlwaysBuild(env.Alias("kfcfw_auto_discovery", None, lambda source, target, env: firmware_config(source, target, env, "auto_discovery")))

env.AddCustomTarget("disassemble", None, [], title="disassemble main prog", description="run objdump to create disassembly", always_build=True)
env.AddCustomTarget("kfcfw_factory", None, [], title="factory reset", description="KFC firmware OTA factory reset", always_build=False)
env.AddCustomTarget("kfcfw_auto_discovery", None, [], title="auto discovery", description="KFC firmware OTA publish auto discovery", always_build=False)

env.AddBuildMiddleware(lambda node: None, '$PROJECT_INCLUDE_DIR/build.h')


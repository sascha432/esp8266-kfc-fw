Import("env")
import datetime
try:
    import configparser
except ImportError:
    import ConfigParser as configparser
from SCons.Script import ARGUMENTS
import subprocess
import os.path
import os
import subprocess
import click
import re
from datetime import datetime
import platform

symlinks = []

def extract_re(regex, filename):
    with open(filename, 'rt') as file:
        contents = file.read()
        contents = contents.replace('\n', ' ').replace('\r', ' ')
        res = {}
        for regex in regex:
            m = re.search(regex, contents)
            if m:
                res.update(m.groupdict())
            else:
                click.secho('Cannot extract %s from %s' % (regex, filename), fg='red')
                exit(1)
        return res

def env_abspath(env, str):
    return os.path.abspath(env.subst(str))

def build_webui(source, target, env, force = False):
    global symlinks

    verbose = int(ARGUMENTS.get("PIOVERBOSE", 0))

    php_bin = env.subst(env.GetProjectOption('custom_php_exe', env.subst('$PHPEXE')))
    if not php_bin:
        if 'PHPEXE' in os.environ:
            php_bin = os.environ['PHPEXE']
        else:
            php_bin = os.path.sep == '\\' and 'php.exe' or 'php'

    php_file = env_abspath(env, '$PROJECT_DIR/lib/KFCWebBuilder/bin/include/cli_tool.php')
    json_file = env_abspath(env, '$PROJECT_DIR/KFCWebBuilder.json')

    build = extract_re([r'#define\s+__BUILD_NUMBER\s+"(?P<build>[0-9]+)"'], env_abspath(env, '$PROJECT_DIR/include/build.h'))
    version = extract_re([r'#define\s+FIRMWARE_VERSION_MAJOR\s+(?P<major>[0-9]+) ', r'#define\s+FIRMWARE_VERSION_MINOR\s+(?P<minor>[0-9]+) ', r'#define\s+FIRMWARE_VERSION_REVISION\s+(?P<rev>[0-9]+) '], env_abspath(env, '$PROJECT_DIR/include/global.h'))
    version = '%s.%s.%s Build %s (%s)' % (version['major'], version['minor'], version['rev'], build['build'], datetime.now().strftime('%b %d %Y %H:%M:%S'))

    with open(env_abspath(env, '$PROJECT_DIR/data/.pvt/build'), 'w') as file:
        file.write(version)

    defines = env.get('CPPDEFINES');
    definesFile = env.subst('$BUILD_DIR/cppdefines.txt');
    with open(definesFile, 'wt') as file:
        for define in defines:
            if isinstance(define, tuple):
                (key, val) = define
            else:
                key = define
                val = 1;
            file.write('%s=%s\n' % (env.subst(key), env.subst(val)))

    if not os.path.isfile(php_bin):
        click.secho('cannot find php binary: %s: set custom_php_exe=<php binary> or environment variable PHPEXE' % php_bin, fg='yellow')
        env.Exit(1)
    if not os.path.isfile(php_file):
        click.secho('cannot find %s' % php_file, fg='yellow')
        env.Exit(1)
    if not os.path.isfile(json_file):
        click.secho('cannot find %s' % json_file, fg='yellow')
        env.Exit(1)

    args = [ php_bin, php_file, json_file, '--branch', 'spiffs', '--env', 'env:%s' % env.subst('$PIOENV'), '--clean-exit-code', '0', '--dirty-exit-code', '0', '--defines-from', definesFile ]


    dirs = []
    for item in defines:
        try:
            if item[0]=='INCLUDE_DATA_DIRS':
                dirs = env.subst(item[1])
                dirs = dirs.split(',')
                break
        except:
            pass

    if len(dirs):
        for src in dirs:
            src = env_abspath(env, src)
            dst = env_abspath(env, '${PROJECTDATA_DIR}/%s' % os.path.basename(src))
            if not os.path.exists(dst):
                if verbose:
                    click.secho('Creating symlink %s -> %s' % (src, dst), fg='yellow')
                if platform.system() == 'Windows':
                    a = [ 'mklink', '/J', dst, src ]
                    return_code = subprocess.run(a, shell=True).returncode
                else:
                    try:
                        os.symlink(src, dst, False)
                        return_code = 0
                    except:
                        return_code = 1
                if return_code:
                    click.secho('Failed to create symlink: [%u] %s -> %s' % (return_code, src, dst), fg='red')
                    env.Exit(1)
        symlinks.append(dst)

    if force:
        args.append('--force')

    # cli_cmd = subprocess.check_output(args, shell=True);
    cli_cmd = ' '.join(args);
    if verbose:
        click.echo(cli_cmd)

    return_code = subprocess.run(args, shell=True).returncode

    if return_code!=0:
        click.secho('failed to run: %s' % cli_cmd)
        print()
        env.Exit(1)

def rebuild_webui(source, target, env):
    build_webui(source, target, env, True)

def before_clean(source, target, env):
    if platform.system() == 'Windows':
        env.Execute("del \"${PROJECTDATA_DIR}/webui/\" -Recurse")
    else:
        env.Execute("rm -fR \"${PROJECTDATA_DIR}/webui/\"")

def build_webui_cleanup(source, target, env):
    global symlinks
    verbose = int(ARGUMENTS.get("PIOVERBOSE", 0))
    if symlinks:
        for lnk in symlinks:
            if verbose:
                click.secho('Removing symlink %s' % lnk, fg='yellow')
            os.remove(lnk)

# env.AddPreAction("$BUILD_DIR/spiffs.bin", build_webui)
env.AddPreAction("$BUILD_DIR/littlefs.bin", build_webui)
env.AddPostAction("$BUILD_DIR/littlefs.bin", build_webui_cleanup)
#env.AddPreAction("buildfs", build_webui)
env.AlwaysBuild(env.Alias("rebuildfs", None, rebuild_webui))
env.AlwaysBuild(env.Alias("buildfs", None, build_webui))


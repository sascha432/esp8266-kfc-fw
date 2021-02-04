Import("env")
import datetime
try:
    import configparser
except ImportError:
    import ConfigParser as configparser
from SCons.Script import ARGUMENTS
import subprocess
import os.path
import subprocess
import click

def build_webui(source, target, env, force = False):

    verbose = int(ARGUMENTS.get("PIOVERBOSE", 0))

    php_bin = env.subst(env.GetProjectOption('custom_php_exe', env.subst('$PHPEXE')))
    if not php_bin:
        if os.environ['PHPEXE']:
            php_bin = os.environ['PHPEXE']
        else:
            php_bin = os.path.sep == '\\' and 'php.exe' or 'php'

    php_file = os.path.abspath(env.subst('$PROJECT_DIR/lib/KFCWebBuilder/bin/include/cli_tool.php'));
    json_file = os.path.abspath(env.subst('$PROJECT_DIR/KFCWebBuilder.json'));

    if not os.path.isfile(php_bin):
        click.secho('cannot find php binary: %s: set custom_php_exe=<php binary>' % php_bin, fg='yellow')
        env.Exit(1)
    if not os.path.isfile(php_file):
        click.secho('cannot find %s' % php_file, fg='yellow')
        env.Exit(1)
    if not os.path.isfile(json_file):
        click.secho('cannot find %s' % json_file, fg='yellow')
        env.Exit(1)

    args = [ php_bin, php_file, json_file, '--branch', 'spiffs', '--env', 'env:%s' % env.subst('$PIOENV'), '--clean-exit-code', '0', '--dirty-exit-code', '0' ]
    if force:
        args.append('--force')

    if verbose:
        click.echo(' '.join(args))

    return_code = subprocess.run(args, shell=True).returncode
    if return_code!=0:
        click.secho('failed to run: %s' % ' '.join(args))
        env.Exit(1)

def rebuild_webui(source, target, env):
    build_webui(source, target, env, True)

def before_clean(source, target, env):
    env.Execute("del ${PROJECTDATA_DIR}/webui/ -Recurse")


env.AddPreAction("$BUILD_DIR/spiffs.bin", build_webui)
#env.AddPreAction("buildfs", build_webui)
env.AlwaysBuild(env.Alias("rebuildfs", None, rebuild_webui))

Import("env")
import sys
from os import path
import os
import platform
import subprocess
import click
from SCons.Node import FS
from SCons.Script import ARGUMENTS, COMMAND_LINE_TARGETS

# -------------------------------------------------------------------------
# build number
#
# increments include/build_number.txt once per `pio run` invocation (all
# environments of one command share the same number) and regenerates
# src/build_number.cpp, see scripts/build_number.py for the file formats
# -------------------------------------------------------------------------

# these do not compile the firmware and must not advance the counter
# - `newbuild` is handled by scripts/extra_script.py and increments explicitly (`--force`)
# - `monitor` does not build anything
BUILD_NUMBER_SKIP_TARGETS = frozenset([
    'buildfs', 'compiledb', 'envdump', 'fullclean', 'monitor', 'newbuild', 'nobuild', 'rebuildfs', 'uploadfs', 'uploadfsota',
])


def generate_build_number():
    # clean runs have no target name, PlatformIO passes FULLCLEAN/--clean instead
    if ARGUMENTS.get('FULLCLEAN') is not None or '--clean' in sys.argv:
        return
    if BUILD_NUMBER_SKIP_TARGETS.intersection(COMMAND_LINE_TARGETS):
        return

    project_dir = env.subst('$PROJECT_DIR')
    scripts_dir = path.join(project_dir, 'scripts')
    sys.path.insert(0, scripts_dir)
    try:
        import build_number
        build_number.main([
            '--project-dir', project_dir,
            '--env', env.subst('$PIOENV'),
            '--verbose',
        ])
    finally:
        sys.path.remove(scripts_dir)


generate_build_number()

# def process_node(node: FS.File):
#     if node:
#         file = node.srcnode().get_abspath()
#         print(file)
#         # exit(1)
#     #return None # skip
#     return node

# env.AddBuildMiddleware(process_node, '*')


# cleans the data dir
def before_clean(source, target, env):
    dir = path.abspath(env.subst('$PROJECTDATA_DIR'))
    webui_dir = path.join(dir, 'webui')
    pvt_dir = path.join(dir, '.pvt')

    if not path.isdir(webui_dir):
        click.secho('cannot clean: %s\n%s does not exist' % (dir, webui_dir), gf='red')
        env.Exit(1)

    if platform.system() == 'Windows':
        args = [ 'del', '/S', '/Q', dir]
        return_code = subprocess.run(args, shell=True).returncode
    else:
        args = [ 'rm', '-fR', dir]
        return_code = subprocess.run(args, shell=False).returncode

    if return_code!=0:
        click.secho('failed to run: %s' % str(args))
        print()
        env.Exit(1)

    os.makedirs(webui_dir, exist_ok=True)
    os.makedirs(pvt_dir, exist_ok=True)


# addPreAction or addPostAction does not work
fullclean = ARGUMENTS.get("FULLCLEAN", None)
if fullclean!=None:
    before_clean(None, None, env)

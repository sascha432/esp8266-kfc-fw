Import("env")
import sys
from os import path
import os
import platform
import subprocess
import click
from SCons.Node import FS

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

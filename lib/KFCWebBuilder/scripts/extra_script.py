Import("env")
import datetime
from pprint import pprint
try:
    import configparser
except ImportError:
    import ConfigParser as configparser
import subprocess
import shutil
import os.path

def build_webui(source, target, env, clean = False):
    args = [
        '%PHPEXE%',
        "lib/KFCWebBuilder/bin/include/cli_tool.php",
        "./KFCWebBuilder.json",
        "-b",
        "spiffs",
        "-e",
        '"env:${PIOENV}"',
        "--clean-exit-code",
        "0"
    ]
    if clean:
        args.append('-f')
    env.Execute(' '.join(args))

def rebuild_webui(source, target, env):
    build_webui(source, target, env, True)

def before_clean(source, target, env):
    env.Execute("del ${PROJECTDATA_DIR}/webui/ -Recurse")


env.AddPreAction("$BUILD_DIR/spiffs.bin", build_webui)
#env.AddPreAction("buildfs", build_webui)
env.AlwaysBuild(env.Alias("rebuildfs", None, rebuild_webui))

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

def modify_upload_command(source, target, env, fs = False):
    if env["UPLOAD_PROTOCOL"]=='espota':
        (login, host) = env["UPLOAD_PORT"].split('@', 2)
        (username, password) = login.split(':', 2)
        if fs:
            cmd = 'uploadfs'
        else:
            cmd = 'upload'
        args = [ '--user', username, '--pass', password, '--image', str(source[0]), cmd, host ]

        env.Replace(UPLOAD_FLAGS=' '.join(args))

def modify_upload_command_fs(source, target, env):
    modify_upload_command(source, target, env, True)

def new_build(source, target, env):
    env.Execute(env['PYTHONEXE'] + " \"${PROJECT_DIR}/scripts/build_number.py\" -v \"${PROJECT_DIR}/include/build.h\"");

def get_last_build_id():
    p = subprocess.Popen([env['PYTHONEXE'], "./scripts/build_number.py", "./include/build_id.h", "--name", "__BUILD_ID", "--print-number", "--add", "-1", "--format", "bid{0:08x}"], stdout=subprocess.PIPE, text=True)
    output, errors = p.communicate()
    if p.wait()==0:
        return output.strip()
    return None

def update_build_id(source, target, env):
    env.Execute(env['PYTHONEXE'] + " \"${PROJECT_DIR}/scripts/build_number.py\" -v \"${PROJECT_DIR}/include/build_id.h\" --name __BUILD_ID");

def git_get_head():
    p = subprocess.Popen(["%GITEXE%", "rev-parse", "HEAD"], stdout=subprocess.PIPE, text=True)
    output, errors = p.communicate()
    if p.wait()==0:
        return output.strip()
    return "NA"

def copy_file(source_path):
    id = get_last_build_id()
    if id!=None:
        target_file = os.path.basename(source_path)
        target_dir = "\\\\192.168.0.3\\kfcfw\\" + id + "_"
        target_path = target_dir + target_file
        shutil.copy2(source_path, target_path)
    try:
        with open(target_dir + "git_head", 'wt') as file:
            file.write(git_get_head())
    except:
        pass

def copy_firmware(source, target, env):
    copy_file(str(target[0]))
    # copy_file(str(target[0]).replace(".elf", ".bin"))
    copy_file("platformio.ini")

def copy_spiffs(source, target, env):
    copy_file(str(target[0]))

env.AddPreAction("upload", modify_upload_command)
env.AddPreAction("uploadota", modify_upload_command)
env.AddPreAction("uploadfs", modify_upload_command_fs)
env.AddPreAction("uploadfsota", modify_upload_command_fs)


env.AlwaysBuild(env.Alias("newbuild", None, new_build))

env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", update_build_id)
# env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", copy_firmware)
# env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_firmware)


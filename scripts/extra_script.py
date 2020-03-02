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

config = {
    "kfcfw_build_dir": "\\\\192.168.0.3\\kfcfw",
    "git_bin": "C:/Users/sascha/AppData/Local/GitHubDesktop/app-1.5.1/resources/app/git/cmd/git",
    "python_bin": "python",
    "php_bin": "php"
}

def build_webui(source, target, env):
    print("build_webui")
    env.Execute(config["php_bin"] + " lib/KFCWebBuilder/bin/include/cli_tool.php ./KFCWebBuilder.json -b spiffs -e env:${PIOENV} --clean-exit-code")

def upload_fs(source, target, env):
    build_webui(source, target, env)
    try:
        values = [str(target[0]), env["UPLOAD_PORT"], "--image=" + str(source[0])]
        for value in env["UPLOAD_FLAGS"]:
            if value[0:6]=="--user" or value[0:6]=="--pass":
                values.append(value);
        env._update({"UPLOAD_FLAGS": values})
    except:
        pass

def rebuild_webui(source, target, env):
    print("rebuild_webui")
    env.Execute(config["php_bin"] + " lib/KFCWebBuilder/bin/include/cli_tool.php ./KFCWebBuilder.json -f -b spiffs -e env:${PIOENV}")

def before_clean(source, target, env):
    print("Cleaning data\webui")
    env.Execute("del ${PROJECTDATA_DIR}/webui/ -Recurse")
    env.Execute(config["php_bin"] + " ${PROJECT_DIR}/lib/KFCWebBuilder/bin/include/cli_tool.php ${PROJECT_DIR}/KFCWebBuilder.json -b spiffs -e env:${PIOENV} --dirty")

def new_build(source, target, env):
    env.Execute(config["python_bin"] + " \"${PROJECT_DIR}/scripts/build_number.py\" -v \"${PROJECT_DIR}/include/build.h\"");

def get_last_build_id():
    p = subprocess.Popen([config["python_bin"], "./scripts/build_number.py", "./include/build_id.h", "--name", "__BUILD_ID", "--print-number", "--add", "-1", "--format", "bid{0:08x}"], stdout=subprocess.PIPE, text=True)
    output, errors = p.communicate()
    if p.wait()==0:
        return output.strip()
    return None

def git_get_head():
    p = subprocess.Popen([config["git_bin"], "rev-parse", "HEAD"], stdout=subprocess.PIPE, text=True)
    output, errors = p.communicate()
    if p.wait()==0:
        return output.strip()
    return "NA"


def update_build_id(source, target, env):
    env.Execute(config["python_bin"] + " \"${PROJECT_DIR}/scripts/build_number.py\" -v \"${PROJECT_DIR}/include/build_id.h\" --name __BUILD_ID");

def copy_file(source_path):
    id = get_last_build_id()
    if id!=None:
        target_file = os.path.basename(source_path)
        target_dir = config["kfcfw_build_dir"] + "\\" + id + "_"
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

# def pre_upload(source, target, env):
#     build_webui(source, target, env)

# def post_upload(source, target, env):
    # record_size(source, target, env)

# env.AddPreAction("uploadfs", pre_upload)
# env.AddPostAction("upload", post_upload)

#env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", record_size)

env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", update_build_id)
env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", copy_firmware)
# env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_firmware)

env.AddPreAction("uploadfs", upload_fs)
# env.AddPostAction("uploadfs", copy_spiffs)

env.AlwaysBuild(env.Alias("webui", None, build_webui))
env.AlwaysBuild(env.Alias("buildfs", None, build_webui))
env.AlwaysBuild(env.Alias("rebuild_webui", None, rebuild_webui))
env.AlwaysBuild(env.Alias("rebuildfs", None, rebuild_webui))
env.AlwaysBuild(env.Alias("newbuild", None, new_build))

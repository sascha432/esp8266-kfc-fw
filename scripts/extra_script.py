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
import sys

def new_build(source, target, env):
    env.Execute(env['PYTHONEXE'] + " \"${PROJECT_DIR}/scripts/build_number.py\" -v \"${PROJECT_DIR}/include/build.h\"");

def modify_upload_command(source, target, env, fs = False):
    if fs==False:
        new_build(source, target, env)
    if env["UPLOAD_PROTOCOL"]=='espota':
        (login, host) = env["UPLOAD_PORT"].split('@', 2)
        (username, password) = login.split(':', 2)

        args = [ '--user', username, '--pass', password, '--image', str(source[0]) ]
        if fs:
            args.append('uploadfs')
            args.append(host)
        else:
            args.append('upload')
            args.append(host)

        args.append('--elf')
        args.append(os.path.realpath(os.path.join(env.subst("$PROJECT_DIR"), 'elf')))
        args.append('--ini')
        args.append(env.subst("$PROJECT_CONFIG"))

        env.Replace(UPLOAD_FLAGS=' '.join(args))

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


env.AddPreAction("upload", modify_upload_command)
env.AddPreAction("uploadota", modify_upload_command)
env.AddPreAction("uploadfs", modify_upload_command_fs)
env.AddPreAction("uploadfsota", modify_upload_command_fs)

env.AlwaysBuild(env.Alias("newbuild", None, new_build))

env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", mem_analyzer)
# env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_firmware)


Import("env")
import datetime

def build_webui(source, target, env):
    print("build_webui")

    env.Execute("php lib\\KFCWebBuilder\\bin\\include\\cli_tool.php .\KFCWebBuilder.json -b spiffs -e env:${PIOENV} --clean-exit-code")

def rebuild_webui(source, target, env):
    print("rebuild_webui")

    env.Execute("php lib\\KFCWebBuilder\\bin\\include\\cli_tool.php .\KFCWebBuilder.json -f -b spiffs -e env:${PIOENV} --clean-exit-code")

def before_clean(source, target, env):

    print("Cleaning data\webui")
    env.Execute("del ${PROJECTDATA_DIR}\\webui\\ -Recurse")
    env.Execute("php ${PROJECT_DIR}\\lib\\KFCWebBuilder\\bin\\include\\cli_tool.php ${PROJECT_DIR}\\KFCWebBuilder.json -b spiffs -e env:${PIOENV} --dirty")

# def pre_upload(source, target, env):
#     build_webui(source, target, env)

# def post_upload(source, target, env):
    # record_size(source, target, env)

#env.Execute("php \"${PROJECT_DIR}\\scripts\\build_number.php\"");

env.AddPreAction("uploadfs", build_webui)
# env.AddPreAction("uploadfs", pre_upload)
# env.AddPostAction("upload", post_upload)

#env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", record_size)

env.AlwaysBuild(env.Alias("webui", None, build_webui))

env.AlwaysBuild(env.Alias("rebuild_webui", None, rebuild_webui))

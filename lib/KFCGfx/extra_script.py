Import('env')
from os.path import join, realpath

for item in env.get("CPPDEFINES", []):
    if isinstance(item, tuple) and item[0] == "HAVE_KFCGFXLIB" and item[1] != 0:
        env.Append(CPPPATH=[realpath("src")])
        env.Replace(SRC_FILTER=["+<../src/>"])
        break

Import('env')

src_filter = ["-<*>"]
for item in env.get("CPPDEFINES", []):
    if isinstance(item, tuple) and item[0] == "HAVE_KFCGFXLIB" and item[1] != 0:
        src_filter = ["+<src>"]
        break

print("SRC_FILTER"
)
print(src_filter)
env.Replace(SRC_FILTER=src_filter)


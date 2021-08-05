Import("env")
import sys
from os import path
from SCons.Node import FS

def process_node(node: FS.File):
    if node:
        file = node.srcnode().get_abspath()
        print(file)
        # exit(1)
    #return None # skip
    return node

# env.AddBuildMiddleware(process_node, '*')

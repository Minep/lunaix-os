#!/usr/bin/env python 
# import sys
# from os.path import relpath, dirname
# sys.path.append(dirname(relpath(__file__)))

from lbuild.contract import LunaBuildFile
from lbuild.common import BuildEnvironment
from lbuild.api import ConfigProvider
from os import getcwd

def main():
    env = BuildEnvironment(getcwd())
    env.set_config_provider(ConfigProvider())

    root = LunaBuildFile(env, "LBuild")

    try:
        root.resolve()
    except Exception as err:
        print("failed to resolve root build file")
        return
    
    env.export_sources("sources.list")

if __name__ == "__main__":
    main()
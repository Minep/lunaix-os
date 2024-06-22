#!/usr/bin/env python 
# import sys
# from os.path import relpath, dirname
# sys.path.append(dirname(relpath(__file__)))

from lbuild.contract import LunaBuildFile
from lbuild.common import BuildEnvironment
from lbuild.api import ConfigProvider
from os import mkdir
from os.path import abspath, basename, dirname, exists
from argparse import ArgumentParser

def main():
    parser = ArgumentParser()
    parser.add_argument("root", nargs="?", default="LBuild")
    parser.add_argument("-o", "--out-dir", required=True)

    opts = parser.parse_args()
    
    root_path = abspath(opts.root)
    ws_path = dirname(root_path)
    root_name = basename(root_path)

    env = BuildEnvironment(ws_path)
    env.set_config_provider(ConfigProvider())

    root = LunaBuildFile(env, root_name)

    try:
        root.resolve()
    except Exception as err:
        print("failed to resolve root build file")
        return
    
    out_dir = opts.out_dir
    if not exists(out_dir):
        mkdir(out_dir)
    
    env.export(out_dir)

if __name__ == "__main__":
    main()
#!/usr/bin/env python 

from lbuild.contract import LunaBuildFile
from lbuild.common import BuildEnvironment

from lcfg.common import LConfigEnvironment
from integration.config_io import CHeaderConfigProvider
from integration.lbuild_bridge import LConfigProvider

import lcfg.types as lcfg_type
import lcfg.builtins as builtin

from os import getcwd
from os import mkdir
from os.path import abspath, basename, dirname, exists, join
from argparse import ArgumentParser

def prepare_lconfig_env(out_dir):
    provider = CHeaderConfigProvider(join(out_dir, "configs.h"))
    env = LConfigEnvironment(getcwd(), provider)

    env.register_builtin_func(builtin.v)
    env.register_builtin_func(builtin.term_type)
    env.register_builtin_func(builtin.parent)
    env.register_builtin_func(builtin.default)
    env.register_builtin_func(builtin.include)

    env.type_factory().regitser(lcfg_type.PrimitiveType)
    env.type_factory().regitser(lcfg_type.MultipleChoiceType)

    return env

def main():
    parser = ArgumentParser()
    parser.add_argument("root", nargs="?", default="LBuild")
    parser.add_argument("-o", "--out-dir", required=True)

    opts = parser.parse_args()
    out_dir = opts.out_dir
    
    root_path = abspath(opts.root)
    ws_path = dirname(root_path)
    root_name = basename(root_path)

    env = BuildEnvironment(ws_path)
    lcfg_env = prepare_lconfig_env(out_dir)

    cfg_provider = LConfigProvider(lcfg_env)
    env.set_config_provider(cfg_provider)

    lcfg_env.resolve_module("LConfig")
    lcfg_env.update()
    lcfg_env.load()

    root = LunaBuildFile(env, root_name)

    try:
        root.resolve()
    except Exception as err:
        print("failed to resolve root build file")
        return
    
    if not exists(out_dir):
        mkdir(out_dir)
    
    lcfg_env.export()
    env.export(out_dir)

if __name__ == "__main__":
    main()
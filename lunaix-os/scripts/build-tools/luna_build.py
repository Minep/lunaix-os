#!/usr/bin/env python 

from lbuild.contract import LunaBuildFile
from lbuild.common import BuildEnvironment

from lcfg.common import LConfigEnvironment
from integration.config_io import CHeaderConfigProvider
from integration.lbuild_bridge import LConfigProvider
from integration.render_ishell import InteractiveShell
from integration.build_gen import MakefileBuildGen, install_lbuild_functions

import lcfg.types as lcfg_type
import lcfg.builtins as builtin

from os import getcwd
from os import mkdir
from os.path import abspath, basename, dirname, exists
from argparse import ArgumentParser
from lib.utils import join_path

def prepare_lconfig_env(out_dir):
    provider = CHeaderConfigProvider(join_path(out_dir, "configs.h"))
    env = LConfigEnvironment(getcwd(), provider)

    env.register_builtin_func(builtin.v)
    env.register_builtin_func(builtin.term_type)
    env.register_builtin_func(builtin.parent)
    env.register_builtin_func(builtin.default)
    env.register_builtin_func(builtin.include)
    env.register_builtin_func(builtin.env)
    env.register_builtin_func(builtin.set_value)

    env.type_factory().regitser(lcfg_type.PrimitiveType)
    env.type_factory().regitser(lcfg_type.MultipleChoiceType)

    return env

def do_config(opt, lcfg_env):
    redo_config = not exists(opt.config_save) or opt.force
    if not redo_config:
        return
    
    shell = InteractiveShell(lcfg_env)
    if not shell.render_loop():
        print("Configuration aborted.")
        exit(-1)

def do_buildfile_gen(opts, lcfg_env):
    root_path = abspath(opts.root)
    ws_path = dirname(root_path)
    root_name = basename(root_path)

    mkgen = MakefileBuildGen(opts.out_dir)
    env = BuildEnvironment(ws_path, mkgen)

    install_lbuild_functions(env)

    cfg_provider = LConfigProvider(lcfg_env)
    env.set_config_provider(cfg_provider)

    root = LunaBuildFile(env, root_name)

    try:
        root.resolve()
    except Exception as err:
        print("failed to resolve root build file")
        raise err
    
    env.export()

def main():
    parser = ArgumentParser()
    parser.add_argument("--config", action="store_true", default=False)
    parser.add_argument("--lconfig-file", default="LConfig")
    parser.add_argument("--config-save", default=".config.json")
    parser.add_argument("--force", action="store_true", default=False)
    parser.add_argument("root", nargs="?", default="LBuild")
    parser.add_argument("-o", "--out-dir", required=True)

    opts = parser.parse_args()
    out_dir = opts.out_dir
    if not exists(out_dir):
        mkdir(out_dir)
    
    lcfg_env = prepare_lconfig_env(out_dir)
    require_config = exists(opts.lconfig_file)
    try:
        if require_config:
            lcfg_env.resolve_module(opts.lconfig_file)
            lcfg_env.update()
            lcfg_env.load()
    except Exception as e:
        print(e)
    
    if opts.config:
        if require_config:
            do_config(opts, lcfg_env)
        else:
            print("No configuration file detected, skipping...")
        lcfg_env.save(opts.config_save)
        lcfg_env.export()
    else:
        do_buildfile_gen(opts, lcfg_env)

if __name__ == "__main__":
    main()
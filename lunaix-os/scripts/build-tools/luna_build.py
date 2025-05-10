#!/usr/bin/env python 
from argparse           import ArgumentParser
from pathlib            import Path

from lbuild.build       import BuildEnvironment
from lbuild.scope       import ScopeProvider
from lcfg2.builder      import NodeBuilder
from lcfg2.config       import ConfigEnvironment
from lcfg2.common       import ConfigNodeError

from shared.export      import ExportJsonFile
from shared.export      import ExportHeaderFile
from shared.export      import ExportMakefileRules
from shared.export      import restore_config_value
from shared.scopes      import ConfigScope
from shared.build_gen   import BuildScriptGenerator
from shared.shconfig    import shconfig

class LunaBuild:
    def __init__(self, options):
        self.__lbuilder = BuildEnvironment()
        self.__lconfig  = ConfigEnvironment()
        self.__opt = options

        scope = ConfigScope(self.__lconfig)
        self.__lbuilder.register_scope(scope)

        scope = ScopeProvider("src")
        scope.file_subscope("c")
        scope.file_subscope("h")
        self.__lbuilder.register_scope(scope)

        scope = ScopeProvider("flag")
        scope.subscope("cc")
        scope.subscope("ld")
        self.__lbuilder.register_scope(scope)

        self.__json  = ExportJsonFile(self.__lconfig)
        self.__make  = ExportMakefileRules(self.__lconfig)
        self.__headr = ExportHeaderFile(self.__lconfig)
        self.__build = BuildScriptGenerator(self.__lbuilder)

    def load(self):
        file = self.__opt.lconfig
        NodeBuilder.build(self.__lconfig, file)

        file = self.__opt.lbuild
        self.__lbuilder.load(file)

        self.__lconfig.refresh()
        self.__lbuilder.update()
    
    def restore(self):
        save = self.__opt.save
        if not Path(save).exists():
            return False

        restore_config_value(self.__lconfig, save)
        return True

    def save(self):
        save = self.__opt.save
        self.__json.export(save)
        return True
    
    def generate(self):
        outdir = Path(self.__opt.export_dir)
        if not outdir.exists():
            outdir.mkdir()

        if self.__opt.gen_build:
            self.__build.generate(outdir / "build.mkinc")

        if self.__opt.gen_config:
            self.__make.export(outdir / "config.mkinc")
            self.__headr.export(outdir / "config.h")

    def visual_config(self):
        if not shconfig(self.__lconfig):
            print("configuration process aborted")
            exit(1)

def main():
    parser = ArgumentParser()
    parser.add_argument("--lconfig", default="LConfig")
    parser.add_argument("--lbuild", default="LBuild")
    parser.add_argument("--save", default=".config.json")
    parser.add_argument("--gen-build", action="store_true", default=False)
    parser.add_argument("--gen-config", action="store_true", default=False)
    parser.add_argument("export_dir")

    opts = parser.parse_args()
    builder = LunaBuild(opts)

    try:
        builder.load()
        builder.restore()
    except ConfigNodeError as e:
        print(e)
        exit(1)
    
    builder.visual_config()
    
    builder.save()
    builder.generate()


if __name__ == "__main__":
    main()
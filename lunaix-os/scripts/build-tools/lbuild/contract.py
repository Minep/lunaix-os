from lib.sandbox import Sandbox
from .common import BuildEnvironment
from lib.utils import join_path

import os

class LunaBuildFile(Sandbox):
    def __init__(self, env: BuildEnvironment, path) -> None:
        super().__init__({
            "_script": 
                path,
            "sources": 
                lambda src: self.export_sources(src),
            "configured":
                lambda name: self.check_config(name),
            "config":
                lambda name: self.read_config(name),
            "use":
                lambda file: self.import_buildfile(file)
        })
        
        self.__srcs = []
        self.__env  = env

        self.__path = path
        self.__dir  = os.path.dirname(path)
        
    def resolve(self):
        self.execute(self.__path)
        self.__process_sources()

    def __process_sources(self):
        resolved = []
        for entry in self.__srcs:
            if not entry:
                continue
            resolved.append(self.__resolve_sources(entry))
        
        self.__env.add_sources(resolved)

    def __resolve_sources(self, source):
        resolved = source
        while not isinstance(resolved, str):
            if isinstance(resolved, dict):
                tests = list(resolved.keys())
                if len(tests) != 1:
                    raise TypeError(
                        "select statement must have exactly one conditional")
                
                test = tests[0]
                outcomes = resolved[test]
                if test not in outcomes:
                    self.__raise("unbounded select")
                resolved = outcomes[test]
            else:
                self.__raise(f"entry with unknown type: {resolved}")
        
        resolved = resolved.strip()
        resolved = join_path(self.__dir, resolved)

        return self.__env.to_wspath(resolved)
    
    def import_buildfile(self, path):
        path = self.__resolve_sources(path)
        path = self.__env.to_wspath(path)
        
        if (os.path.isdir(path)):
            path = os.path.join(path, "LBuild")
        
        if not os.path.exists(path):
            self.__raise("Build file not exist: %s", path)

        if os.path.abspath(path) == os.path.abspath(self.__path):
            self.__raise("self dependency detected")

        LunaBuildFile(self.__env, path).resolve()

    def export_sources(self, src_list):
        self.__srcs += src_list

    def check_config(self, name):
        return self.__env.config_provider().has_config(name)
    
    def read_config(self, name):
        return self.__env.config_provider().configured_value(name)
    
    def __raise(self, msg, *kargs):
        raise Exception(msg%kargs)
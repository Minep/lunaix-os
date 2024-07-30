from lib.sandbox import Sandbox
from lib.utils import join_path
from .common import BuildEnvironment

import os

class LunaBuildFile(Sandbox):
    def __init__(self, env: BuildEnvironment, path) -> None:
        super().__init__({
            "_script": 
                path,
            "sources": 
                lambda src: self.export_sources(src),
            "headers":
                lambda hdr: self.export_headers(hdr),
            "configured":
                lambda name: self.check_config(name),
            "config":
                lambda name: self.read_config(name),
            "use":
                lambda file: self.import_buildfile(file),
            **env.external_func_table()
        })
        
        self.__srcs = []
        self.__hdrs = []
        self.__env  = env

        self.__path = path
        self.__dir  = os.path.dirname(path)
        
    def resolve(self):
        self.execute(self.__path)
        self.__env.add_sources(self.__srcs)
        self.__env.add_headers(self.__hdrs)

    def __do_process(self, list):
        resolved = []
        for entry in list:
            if not entry:
                continue
            resolved.append(self.__resolve_value(entry))
        return resolved

    def expand_select(self, val):
        tests = list(val.keys())
        if len(tests) != 1:
            raise TypeError(
                "select statement must have exactly one conditional")
        
        test = tests[0]
        outcomes = val[test]
        if test not in outcomes:
            self.__raise("unbounded select")
        return outcomes[test]

    def __resolve_value(self, source):
        resolved = source
        while isinstance(resolved, dict):
            if isinstance(resolved, dict):
                resolved = self.expand_select(resolved)
            else:
                self.__raise(f"entry with unknown type: {resolved}")
        
        if isinstance(resolved, list):
            rs = []
            for file in resolved:
                file = join_path(self.__dir, file)
                file = self.__env.to_wspath(file)
                rs.append(file)
        else:
            rs = join_path(self.__dir, resolved)
            rs = [self.__env.to_wspath(rs)]

        return rs
    
    def import_buildfile(self, path):
        path = self.__resolve_value(path)
        for p in path:
            if (os.path.isdir(p)):
                p = os.path.join(p, "LBuild")
            
            if not os.path.exists(p):
                self.__raise("Build file not exist: %s", p)

            if os.path.abspath(p) == os.path.abspath(self.__path):
                self.__raise("self dependency detected")

            LunaBuildFile(self.__env, p).resolve()

    def export_sources(self, src):
        src = self.__resolve_value(src)
        self.__srcs += src

    def export_headers(self, hdr):
        hdr = self.__resolve_value(hdr)
        self.__hdrs += hdr

    def check_config(self, name):
        return self.__env.config_provider().has_config(name)
    
    def read_config(self, name):
        return self.__env.config_provider().configured_value(name)
    
    def __raise(self, msg, *kargs):
        raise Exception(msg%kargs)
from lib.utils import join_path
import os

class BuildEnvironment:
    def __init__(self, workspace_dir, generator) -> None:
        self.__config_provider = None
        self.__sources = []
        self.__headers = []
        self.__inc_dir = []
        self.__ws_dir = workspace_dir
        self.__ext_object = {}
        self.__ext_function = {}
        self.__generator = generator

    def set_config_provider(self, provider):
        self.__config_provider = provider
    
    def config_provider(self):
        return self.__config_provider
    
    def add_sources(self, sources):
        self.__sources += sources

    def add_headers(self, sources):
        for h in sources:
            if not os.path.isdir(h):
                self.__headers.append(h)
            else:
                self.__inc_dir.append(h)

    def to_wspath(self, file):
        path = join_path(self.__ws_dir, file)
        return os.path.relpath(path, self.__ws_dir)
    
    def export(self):
        self.__generator.generate(self)

    def get_object(self, key, _default=None):
        return _default if key not in self.__ext_object else self.__ext_object[key]

    def set_object(self, key, object):
        self.__ext_object[key] = object

    def srcs(self):
        return list(self.__sources)
    
    def headers(self):
        return list(self.__headers)
    
    def includes(self):
        return list(self.__inc_dir)
    
    def add_external_func(self, function):
        name = function.__name__
        invk = lambda *args, **kwargs: function(self, *args, **kwargs)
        self.__ext_function[name] = invk

    def external_func_table(self):
        return self.__ext_function
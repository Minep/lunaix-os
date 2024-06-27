from lib.utils import join_path
import os

class BuildEnvironment:
    def __init__(self, workspace_dir) -> None:
        self.__config_provider = None
        self.__sources = []
        self.__headers = []
        self.__inc_dir = []
        self.__ws_dir = workspace_dir

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
    
    def export(self, out_dir):
        path = os.path.join(out_dir, "sources.list")
        with open(path, "w") as f:
            f.write("\n".join(self.__sources))

        path = os.path.join(out_dir, "headers.list")
        with open(path, "w") as f:
            f.write("\n".join(self.__headers))

        path = os.path.join(out_dir, "includes.list")
        with open(path, "w") as f:
            f.write("\n".join(self.__inc_dir))
from lib.utils import join_path
import os

class BuildEnvironment:
    def __init__(self, workspace_dir) -> None:
        self.__config_provider = None
        self.__sources = []
        self.__ws_dir = workspace_dir

    def set_config_provider(self, provider):
        self.__config_provider = provider
    
    def config_provider(self):
        return self.__config_provider
    
    def add_sources(self, sources):
        self.__sources += sources

    def to_wspath(self, file):
        path = join_path(self.__ws_dir, file)
        return os.path.relpath(path, self.__ws_dir)
    
    def export_sources(self, out_file):
        with open(out_file, "w") as f:
            f.write("\n".join(self.__sources))
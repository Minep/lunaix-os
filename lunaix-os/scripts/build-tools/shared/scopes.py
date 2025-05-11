import os

from lbuild.scope import ScopeProvider
from lcfg2.common import NodeProperty

class ConfigScope(ScopeProvider):
    def __init__(self, env):
        super().__init__("config")
        self.__env = env

    def __getitem__(self, name):
        node = self.__env.get_node(name)
        if node is None:
            raise Exception(f"config '{name}' not exists")
        
        if not NodeProperty.Enabled[node]:
            return None

        return NodeProperty.Value[node]

class EnvScope(ScopeProvider):
    def __init__(self):
        super().__init__("env")

    def __getitem__(self, name):
        return os.environ.get(name)
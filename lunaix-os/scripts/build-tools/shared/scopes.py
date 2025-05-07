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
        
        return NodeProperty.Value[node]
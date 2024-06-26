from lbuild.api import ConfigProvider
from lcfg.common import LConfigEnvironment

class LConfigProvider(ConfigProvider):
    def __init__(self, lcfg_env: LConfigEnvironment) -> None:
        super().__init__()
        self.__env = lcfg_env

    def configured_value(self, name):
        return self.__env.lookup_value(name)
    
    def has_config(self, name):
        try:
            self.__env.lookup_value(name)
            return True
        except:
            return False
from lcfg.api import ConfigIOProvider
from lcfg.utils import is_basic

import re

class CHeaderConfigProvider(ConfigIOProvider):
    def __init__(self, header_save, 
                 header_prefix="CONFIG_") -> None:
        self.__header_export = header_save
        self.__prefix = header_prefix
        self.__re = re.compile(r"^[A-Za-z0-9_]+$")

    def export(self, env, config_dict):
        lines = []
        for k, v in config_dict.items():
            result = [ "#define" ]
            s = str.upper(k)
            s = f"{self.__prefix}{s}"

            if isinstance(v, str) and self.__re.match(v):
                s = f"{s}_{str.upper(v)}"
                v = ""
    
            result.append(s)
            
            v = self.serialize_value(v)
            if v is None or v is False:
                result.insert(0, "//")
            elif not isinstance(v, bool):
                result.append(v)

            lines.append(" ".join(result))
            
        with open(self.__header_export, 'w') as f:
            f.write("\n".join(lines))


    def serialize_value(self, v):
        if v is None:
            return None
        
        if isinstance(v, bool):
            return v
        
        if v and isinstance(v, str):
            return f'"{v}"'
        
        if is_basic(v):
            return str(v)
        
        raise ValueError(
                f"serialising {type(v)}: not supported")

from lcfg2.config import ConfigEnvironment
from lcfg2.common import NodeProperty
from lib.utils import SourceLogger
import json
import re

Identifier = re.compile(r"^[A-Za-z_][A-Za-z0-9_]+$")

class Exporter:
    def __init__(self, env: ConfigEnvironment):
        self._env = env

    def export(self, file):
        lines = []

        for node in self._env.terms():
            for name, val in self.variants(node):
                line = self._format_line(node, name, val)
                lines.append(line)

        with open(file, 'w') as f:
            f.write('\n'.join(lines))

    def _format_line(self, node, name, value):
        return ""

    def variants(self, node):
        value    = self.get_value(node)
        basename = f"CONFIG_{str.upper(node._name)}"

        return [(basename, value)]
    
    def get_value(self, node):
        return ""


class ExportMakefileRules(Exporter):
    def __init__(self, env: ConfigEnvironment):
        super().__init__(env)

    def _format_line(self, node, name, value):
        if not node.enabled():
            return f"# {name} is disabled"
        else:
            return f"{name} := {value}"

    def get_value(self, node):
        tok = NodeProperty.Token[node]
        val = NodeProperty.Value[node]

        if type(val) is str:
            return val

        if type(val) is int:
            return val
        
        if type(val) is bool:
            return ""
        
        SourceLogger.warn(node, tok, 
            f"value: '{val}' can not mapped to makefile primitives")
        
        return "## ERROR ##"
    
class ExportHeaderFile(Exporter):
    def __init__(self, env: ConfigEnvironment):
        super().__init__(env)

    def _format_line(self, node, name, value):
        if not node.enabled():
            return f"// {name} is disabled"
        else:
            return f"#define {name} {value}"

    def get_value(self, node):
        tok = NodeProperty.Token[node]
        val = NodeProperty.Value[node]

        if type(val) is str:
            return f'"{val}"'

        if type(val) is int:
            return val
        
        if type(val) is bool:
            return ""
        
        SourceLogger.warn(node, tok, 
            f"value: '{val}' can not mapped to C primitives")
        
        return "// Error"

class ExportJsonFile(Exporter):
    def __init__(self, env):
        super().__init__(env)

    def export(self, file):
        obj = {}
        for n in self._env.terms():
            obj[n._name] = NodeProperty.Value[n]
        
        with open(file, 'w') as f:
            json.dump(obj, f)

def restore_config_value(env: ConfigEnvironment, json_path):
    with open(json_path) as f:
        j = json.load(f)
        for k, v in j.items():
            node = env.get_node(k)
            if node is None:
                print(f"warning: missing node: '{node}', skipped")
                continue

            NodeProperty.Value[node] = v

        env.refresh()
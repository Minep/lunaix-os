from lcfg2.config import ConfigEnvironment
from lcfg2.common import NodeProperty, NodeDependency

import pydoc
import readline, textwrap

class ShconfigException(Exception):
    def __init__(self, *args):
        super().__init__(*args)

    def __str__(self):
        return str(self.args[0])

def select(val, _if, _else):
    return _if if val else _else

def get_node(env: ConfigEnvironment, name: str):
    node_name = name.removeprefix("CONFIG_").lower()
    node = env.get_node(node_name)
    if node is None:
        raise ShconfigException(f"no such config: {name}")
    return node

def show_configs(env: ConfigEnvironment):
    aligned = 40
    lines = [
        "Display format:",
        "",
        "        (flags) CONFIG_NAME ..... VALUE",
        " ",
        "   (flags)",
        "      x    Config is disabled",
        "      r    Read-Only config",
        "      h    Hidden config",
        "",
        "",
        "Defined configuration terms",
        ""
    ]

    for node in env.terms():
        name    = f"CONFIG_{node._name.upper()}"
        value   = NodeProperty.Value[node]
        enabled = NodeProperty.Enabled[node]
        hidden  = NodeProperty.Hidden[node]
        ro      = NodeProperty.Readonly[node]

        status  = f"{select(not enabled, 'x', '.')}"          \
                  f"{select(ro, 'r', '.')}"          \
                  f"{select(hidden, 'h', '.')}"      \
                                    
        val_txt = f"{value if value is not None else '<?>'}"
                  
        line = f"[{status}] {name}"
        to_pad = max(aligned - len(line), 4)
        lines.append(f"    {line} {'.' * to_pad} {val_txt}")    

    pydoc.pager("\n".join(lines))

def set_config(env: ConfigEnvironment, name: str, value):
    node_name = name.removeprefix("CONFIG_").lower()
    node = env.get_node(node_name)
    if node is None:
        raise ShconfigException(f"no such config: {name}")
    
    NodeProperty.Value[node] = value
    env.refresh()

def __print_dep_recursive(env, node, level = 0):
    indent = " "*(level * 4)
    dep: NodeDependency = NodeProperty.Dependency[node]

    print(indent, f"+ {node._name} -> {NodeProperty.Value[node]}")
    if dep is None:
        return
    
    print(indent, f"= {dep._expr}")
    for name in dep._names:
        n = env.get_node(name)
        __print_dep_recursive(env, n, level + 1)

def show_dependency(env: ConfigEnvironment, name: str):
    node = get_node(env, name)
    print(node._name)
    __print_dep_recursive(env, node)

CMDS = {
    "show": lambda env, *args: show_configs(env),
    "dep": lambda env, name, *args: show_dependency(env, name)
}

def next_input(env: ConfigEnvironment):
    line = input("shconfig> ")
    parts = line.split(' ')

    if len(parts) == 0:
        return True
    
    if parts[0] in ['q', 'exit']:
        return False
    
    if parts[0].startswith("CONFIG_"):
        if len(parts) != 2:
            raise ShconfigException("expect a value")
        set_config(env, parts[0], parts[1])
        return True

    cmd = CMDS.get(parts[0])
    if cmd is None:
        raise ShconfigException(f"unknow command: {parts[0]}")
    
    cmd(env, *parts[1:])
    return True

def shconfig(env: ConfigEnvironment):
    print(textwrap.dedent(
        """ 
        Lunaix shconfig
          
        Usage:
            show
                List all configuration

            dep <config>
                Examine dependency chain for <config>
          
            <config> <value>
                Update value of <config> to <value>

        """
    ).strip())
    print()

    while True:
        try:
            if not next_input(env):
                return True
        except ShconfigException as e:
            print(str(e))
            continue
        except KeyboardInterrupt as e:
            return False
        except Exception as e:
            raise e
            return False

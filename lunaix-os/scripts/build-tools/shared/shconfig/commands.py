import textwrap
import pydoc

from .common import CmdTable, ShconfigException
from .common import select, cmd

from lcfg2.config   import ConfigEnvironment
from lcfg2.common   import NodeProperty, NodeDependency

class Commands(CmdTable):
    def __init__(self, env: ConfigEnvironment):
        super().__init__()

        self.__env = env

    def __get_node(self, name: str):
        node_name = name.removeprefix("CONFIG_").lower()
        node = self.__env.get_node(node_name)
        if node is None:
            raise ShconfigException(f"no such config: {name}")
        return node
    
    def __get_opt_line(self, node):
        aligned = 40
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
        return f"{line} {'.' * to_pad} {val_txt}"
    
    @cmd("help", "h")
    def __fn_help(self):
        """
        Print this message
        """

        print()
        for exe in self._cmd_map:
            print(exe, "\n")

    @cmd("show", "ls")
    def __fn_show(self):
        """
        Show all configurable options
        """
        
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

        for node in self.__env.terms():
            lines.append(self.__get_opt_line(node))    

        pydoc.pager("\n".join(lines))

    @cmd("set")
    def __fn_set(self, name: str, value):
        """
        Update a configurable option's value
        """
        
        node = self.__get_node(name)
        if node is None:
            raise ShconfigException(f"no such config: {name}")
        
        NodeProperty.Value[node] = value
        self.__env.refresh()

    @cmd("dep")
    def __fn_dep(self, name: str):
        """
        Show the dependency chain and boolean conditionals
        """

        def __print_dep_recursive(env, node, level = 0):
            indent = " "*(level * 4)
            dep: NodeDependency = NodeProperty.Dependency[node]

            print(f"{indent}+ {node._name} -> {NodeProperty.Value[node]}")
            if dep is None:
                return
            
            print(f"{indent}= {dep._expr}")
            for name in dep._names:
                n = env.get_node(name)
                __print_dep_recursive(env, n, level + 1)

        node = self.__get_node(name)
        print(node._name)
        __print_dep_recursive(self.__env, node)

    @cmd("opt", "val", "v")
    def __fn_opt(self, name: str):
        """
        Show the current value and flags of selected options
        """

        node = self.__get_node(name)        
        print(self.__get_opt_line(node))

    @cmd("what", "?")
    def __fn_what(self, name: str):
        """
        Show the documentation associated with the option
        """

        node = self.__get_node(name)
        help = NodeProperty.HelpText[node]
        help = "<no help message>" if not help else help

        print()
        print(textwrap.indent(help.strip(), "  |\t", lambda _:True))
        print()
    
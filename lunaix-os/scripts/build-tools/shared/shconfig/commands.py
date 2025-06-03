import textwrap
import pydoc
import re

from .common import CmdTable, ShconfigException
from .common import select, cmd, get_config_name

from lcfg2.config   import ConfigEnvironment
from lcfg2.common   import NodeProperty, NodeDependency, ConfigNodeError

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
    
    def __get_opt_line(self, node, color_hint = False):
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

        if value is True:
            val_txt = "y"
        elif value is False:
            val_txt = "n"
        elif isinstance(value, str):
            val_txt = f'"{val_txt}"'
                
        line = f"[{status}] {name}"
        to_pad = max(aligned - len(line), 4)
        line = f"{line} {'.' * to_pad} {val_txt}"

        if color_hint and not enabled:
            line = f"\x1b[90;49m{line}\x1b[0m"
        return line
    
    def __format_config_list(self, nodes):
        lines = []
        disabled = []
        
        for node in nodes:
            _l = disabled if not NodeProperty.Enabled[node] else lines
            _l.append(self.__get_opt_line(node, True))

        if disabled:
            lines += [
                "",
                "\t---- disabled ----",
                "",
                *disabled
            ]

        return lines
    
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
            "   VALUE (bool)",
            "      y    True",
            "      n    False",
            "",
            "",
            "Defined configuration terms",
            ""
        ]

        lines += self.__format_config_list(self.__env.terms())

        pydoc.pager("\n".join(lines))

    @cmd("set")
    def __fn_set(self, name: str, value):
        """
        Update a configurable option's value
        """
        
        node = self.__get_node(name)
        if node is None:
            raise ShconfigException(f"no such config: {name}")
        
        if NodeProperty.Readonly[node]:
            raise ShconfigException(f"node is read only")
        
        try:
            NodeProperty.Value[node] = value
            self.__env.refresh()
        except ConfigNodeError as e:
            print(e)

    @cmd("dep")
    def __fn_dep(self, name: str):
        """
        Show the dependency chain and boolean conditionals
        """

        def __print_dep_recursive(env, node, inds = 0):
            indent = " "*inds
            dep: NodeDependency = NodeProperty.Dependency[node]
            
            state = 'enabled' if NodeProperty.Enabled[node] else 'disabled'
            print(f"{indent}* {node._name} (currently {state})")
            if dep is None:
                return
            
            print(f"  {indent}predicate: {dep._expr}")
            print(f"  {indent}dependents:")
            for name in dep._names:
                n = env.get_node(name)
                __print_dep_recursive(env, n, inds + 6)

        node = self.__get_node(name)
        __print_dep_recursive(self.__env, node)

    @cmd("opt", "val", "v")
    def __fn_opt(self, name: str):
        """
        Show the current value and flags of selected options
        """

        node = self.__get_node(name)        
        print(self.__get_opt_line(node))

    @cmd("what", "help", "?")
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


    @cmd("affects")
    def __fn_affect(self, name: str):
        """
        Show the effects of this option on other options
        """
        
        node = self.__get_node(name)
        link = NodeProperty.Linkage[node]

        if not link:
            return
        
        for other, exprs in link.linkages():
            print(f" {other}:")
            for expr in exprs:
                print(f"   > when {expr}")
            print()
    
    @cmd("find")
    def __fn_search(self, fuzz: str):
        """
        Perform fuzzy search on configs (accept regex)
        """

        nodes = []
        expr = re.compile(fuzz)
        for node in self.__env.terms():
            name = get_config_name(node._name)
            if not expr.findall(name):
                continue
            nodes.append(node)

        if not nodes:
            print("no matches")
            return

        lines = self.__format_config_list(nodes)

        pydoc.pager("\n".join(lines))

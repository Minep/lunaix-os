import ast, os

from lib.utils import ConfigAST
from .common import NodeProperty, NodeDependency
from .nodes import GroupNode, TermNode


class ConfigEnvironment:
    def __init__(self):
        self.__node_table = {}
        self.__exec = None
        self.__globals = {}
        self.__custom_fn = {
            "env": lambda x: os.environ.get(x)
        }
    
    def check_dependency(self, node) -> bool:
        dep = NodeProperty.Dependency[node]
        
        if not isinstance(dep, NodeDependency):
            return node.enabled()
        
        value_map = {}
        for name in dep._names:
            if name not in self.__node_table:
                raise node.config_error(f"config: '{name}' does not exists")
            
            n = self.__node_table[name]
            value_map[name] = self.check_dependency(n)

        return node.enabled() and dep.evaluate(value_map)
    
    def register_node(self, cfg_node):
        name = cfg_node._name
        if name in self.__node_table:
            raise cfg_node.config_error(f"redefinition of '{name}'")
        
        self.__node_table[name] = cfg_node
    
    def get_node(self, name):
        return self.__node_table.get(name)
    
    def relocate_children(self):
        for node in self.__node_table.values():
            parent = NodeProperty.Parent[node]
            if not parent:
                continue

            if not isinstance(parent, str):
                continue
            
            if parent not in self.__node_table:
                raise Exception(node, "unknow parent: %s"%(parent))
            
            parent_node = self.__node_table[parent]
            if not isinstance(parent_node, GroupNode):
                raise Exception(node, "not a valid parent: %s"%(parent))
            
            parent_node.add_child(node)

    def set_exec_context(self, astns):
        filtered = []
        for x in astns:
            if isinstance(x, ConfigAST.EnterFileMarker):
                continue
            if isinstance(x, ConfigAST.LeaveFileMarker):
                continue
            filtered.append(x)

        module = ast.Module(filtered, type_ignores=[])
        module = ast.fix_missing_locations(module)

        self.__exec = compile(module, "", mode='exec')

    def __update_globals(self):
        g = {}

        exec(self.__exec, g)
        self.__globals = {
            **g,
            **self.__custom_fn
        }

    def context(self):
        return self.__globals
        
    def refresh(self):
        self.__update_globals()

        for name, node in self.__node_table.items():
            node.update()

        for name, node in self.__node_table.items():
            node.sanity_check()
            NodeProperty.Enabled[node] = self.check_dependency(node)

    def reset(self):
        self.__node_table.clear()

    def nodes(self):
        for node in self.__node_table.values():
            yield node

    def top_levels(self):
        for node in self.__node_table.values():
            if NodeProperty.Parent[node] is None:
                yield node

    def terms(self):
        for node in self.__node_table.values():
            if isinstance(node, TermNode):
                yield node

import json
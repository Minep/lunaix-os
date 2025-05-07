import ast

from .common import NodeProperty
from lib.utils import Schema, SourceLogger

class LazyLookup:
    def __init__(self):
        self.__tab = {}
    
    def put(self, lazy):
        self.__tab[lazy.get_key()] = lazy

    def get(self, key):
        return self.__tab[key] if key in self.__tab else None
    
    def __setitem__(self, key, val):
        lz = self.__tab[key]
        lz.resolve_set(val)

    def __getitem__(self, key):
        lz = self.__tab[key]
        return lz.resolve_get()

class Lazy:
    NodeValue     = 'val'

    LazyTypes = Schema.Union(NodeValue)
    Syntax  = Schema(ast.Attribute, attr=LazyTypes, value=ast.Name)

    def __init__(self, source, type, target, env):
        self.target = target
        self.type   = type
        self.env    = env
        self.source = source

    def __resolve_type(self):
        if self.type == Lazy.NodeValue:
            return NodeProperty.Value
        return None
        
    def resolve_get(self):
        node = self.env.get_node(self.target)

        accessor = self.__resolve_type()
        if not accessor:
            return None
        
        status = NodeProperty.Status[node]
        if status == "Updating":
            tok = NodeProperty.Token[self.source]
            SourceLogger.warn(self.source, tok, 
                    f"cyclic dependency detected: {self.source._name} <-> {self.target}." + 
                    f"Reusing cached value, maybe staled.")
        else:
            node.update()

        return accessor[node] if accessor else None
    
    def resolve_set(self, val):
        node = self.env.get_node(self.target)
        accessor = self.__resolve_type()

        if NodeProperty.Readonly[node]:
            raise self.source.config_error(
                f"{self.target} is readonly")

        if accessor:
            accessor[node] = val
        else:
            raise self.source.config_error(
                f"invalid type {self.type} for {self.target}")

    def get_key(self):
        return Lazy.get_key_from(self.type, self.target)
    
    @staticmethod
    def get_key_from(type, target):
        return f"{type}${target}"
    
    @staticmethod
    def from_astn(cfgnode, astn):
        if Lazy.Syntax != astn:
            return None
        
        type_ = astn.attr
        target = astn.value.id
        key = Lazy.get_key_from(type_, target)

        lz = cfgnode._lazy_table.get(key)
        if lz:
            return key

        lz = Lazy(cfgnode, type_, target, cfgnode._env)
        cfgnode._lazy_table.put(lz)

        return key

import os.path as path
import ast, json

from .lcnodes import LCModuleNode
from .api import (
    ConfigLoadException, 
    Renderable
)

class LConfigFile:
    def __init__(self, 
                 env, 
                 file) -> None:
        self.__env = env
        self.__file = env.to_wspath(file)

        with open(file) as f:
            tree = ast.parse(f.read(), self.__file, mode='exec')
            self.__module = LCModuleNode(self, tree)

    def filename(self):
        return self.__file
    
    def env(self):
        return self.__env
    
    def root_module(self):
        return self.__module
    
    def compile_astns(self, astns):
        if not isinstance(astns, list):
            astns = [astns]

        return compile(
            ast.Module(body=astns, type_ignores=[]), 
            self.__file, mode='exec')
    
class DependencyGraph:
    def __init__(self) -> None:
        self._edges = {}

    def add(self, dependent, dependee):
        if dependent in self._edges:
            if dependee in self._edges[dependent]:
                return
            self._edges[dependent].add(dependee)
        else:
            self._edges[dependent] = set([dependee])
        
        if self.__check_loop(dependee):
            raise ConfigLoadException(
                f"loop dependency detected: {dependent.get_name()}",
                dependee)

    def __check_loop(self, start):
        visited = set()
        q = [start]
        while len(q) > 0:
            current = q.pop()
            if current in visited:
                return True
            
            visited.add(current)
            if current not in self._edges:
                continue
            for x in self._edges[current]:
                q.append(x)
        
        return False
    
    def cascade(self, start):
        q = [start]
        while len(q) > 0:
            current = q.pop()
            if current in self._edges:
                for x in self._edges[current]:
                    q.append(x)
            current.evaluate()

class ConfigTypeFactory:
    def __init__(self) -> None:
        self.__providers = []

    def regitser(self, provider_type):
        self.__providers.append(provider_type)

    def create(self, typedef):
        for provider in self.__providers:
            if not provider.typedef_matched(typedef):
                continue
            return provider(typedef)
        
        raise ConfigLoadException(
                f"no type provider defined for type: {typedef}", None)

class LConfigEvaluationWrapper:
    def __init__(self, env, node) -> None:
        self.__env = env
        self.__node = node

    def __enter__(self):
        self.__env.push_executing_node(self.__node)
        return self

    def __exit__(self, type, value, tb):
        self.__env.pop_executing_node()

    def evaluate(self):
        return self.__env.evaluate_node(self.__node)

class LConfigEnvironment(Renderable):
    def __init__(self, workspace, config_io) -> None:
        super().__init__()
        
        self.__ws_path = path.abspath(workspace)
        self.__exec_globl = globals()
        self.__eval_stack = []
        self.__lc_modules = []
        self.__config_val = {}
        self.__node_table = {}
        self.__deps_graph = DependencyGraph()
        self.__type_fatry = ConfigTypeFactory()
        self.__config_io  = config_io

    def to_wspath(self, _path):
        _path = path.abspath(_path)
        return path.relpath(_path, self.__ws_path)

    def register_builtin_func(self, func_obj):
        call = (lambda *arg, **kwargs:
                     func_obj(self, *arg, **kwargs))
        
        self.__exec_globl[func_obj.name] = call

    def resolve_module(self, file):
        fo = LConfigFile(self, file)
        self.__lc_modules.insert(0, (fo.root_module()))

    def evaluate_node(self, node):
        name = node.get_name()
        
        return self.get_symbol(name)()
    
    def eval_context(self, node):
        return LConfigEvaluationWrapper(self, node)
    
    def push_executing_node(self, node):
        self.__eval_stack.append(node)

    def pop_executing_node(self):
        return self.__eval_stack.pop()

    def register_node(self, node):
        self.__node_table[node.get_name()] = node

        l = {}
        try:
            self.push_executing_node(node)
            exec(node.get_co(), self.__exec_globl, l)
            self.pop_executing_node()
        except Exception as e:
            raise ConfigLoadException("failed to load", node, e)
        
        self.__exec_globl.update(l)

    def lookup_node(self, name):
        if name not in self.__node_table:
            raise ConfigLoadException(f"node '{name}' undefined")
        return self.__node_table[name]

    def type_factory(self):
        return self.__type_fatry
    
    def update_value(self, key, value):
        self.__config_val[key] = value

    def get_symbol(self, name):
        if name not in self.__exec_globl:
            raise ConfigLoadException(f"unknown symbol '{name}'")
        return self.__exec_globl[name]

    def callframe_at(self, traverse_level):
        try:
            return self.__eval_stack[traverse_level - 1]
        except:
            return None
    
    def lookup_value(self, key):
        return self.__config_val[key]
    
    def dependency(self):
        return self.__deps_graph
    
    def update(self):
        for mod in self.__lc_modules:
            mod.evaluate()
    
    def export(self):
        self.__config_io.export(self, self.__config_val)
    
    def save(self, _path = ".config.json"):
        data = {}
        for mod in self.__lc_modules:
            mod.serialise(data)

        with open(_path, 'w') as f:
            json.dump(data, f)

    def load(self, _path = ".config.json"):
        if not path.exists(_path):
            return
        
        data = {}
        with open(_path, 'r') as f:
            data.update(json.load(f))
        
        for mod in self.__lc_modules:
            mod.deserialise(data)

        self.update()

    def render(self, rctx):
        for mod in self.__lc_modules:
            mod.render(rctx)
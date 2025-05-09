import inspect
import textwrap
import re

from typing         import Callable, Any
from lib.utils      import Schema

def select(val, _if, _else):
    return _if if val else _else

def get_config_name(name):
    return f"CONFIG_{name.upper()}"

def cmd(name, *alias):
    def __cmd(fn: Callable):
        fn.__annotations__["__CMD__"] = True
        fn.__annotations__["__NAME__"] = name
        fn.__annotations__["__ALIAS__"] = [*alias]
        return fn
    return __cmd

def imply_schema(fn: Callable):
    type_map = inspect.get_annotations(fn)
    arg_list = []

    for arg in inspect.getargs(fn.__code__).args:
        if arg == "self":
            continue

        t = type_map.get(arg)
        t = t if t else Any

        arg_list.append((arg, t))
    
    return Schema([x[1] for x in arg_list]), arg_list

class ShconfigException(Exception):
    def __init__(self, *args):
        super().__init__(*args)

    def __str__(self):
        return str(self.args[0])

class Executor:
    def __init__(self, body: Callable):
        self.name = body.__annotations__["__NAME__"]
        self.alias = body.__annotations__["__ALIAS__"]
        self.help = inspect.getdoc(body)
        self.help = textwrap.dedent(self.help if self.help else "")
        
        schema, args = imply_schema(body)
        self.argstr  = ' '.join(
            [f'<{n.upper()}: {t.__name__}>' for n, t in args])
        
        self.__fn = body
        self.__argtype = schema

    def match_name(self, name):
        return self.name == name or name in self.alias

    def try_invoke(self, *args):
        t_args = [self.__type_mapper(x) for x in args]
        if self.__argtype != t_args:
            raise ShconfigException(
                f"invalid parameter ({args}), expect: ({self.argstr})")
    
        self.__fn(*t_args)

    def __type_mapper(self, strtype):
        if strtype in ['True', 'False']:
            return bool(strtype)
        if strtype in ['y', 'n']:
            return bool(strtype == 'y')
        
        try: return int(strtype)
        except: pass

        return strtype
    
    def __str__(self):
        return '\n'.join([
            *[f"{name} {self.argstr}" for name in self.alias + [self.name]],
            textwrap.indent(self.help, '\t')
        ])

TypeRe = re.compile(r"^.*?__fn_(.+)$")
class CmdTable:     
    def __init__(self):
        self._cmd_map = []

        fns = inspect.getmembers(self, 
                                 lambda p: isinstance(p, Callable))
        for _, fn in fns:
            if not isinstance(fn, Callable):
                continue
            if not hasattr(fn, "__annotations__"):
                continue
            if "__CMD__" not in fn.__annotations__:
                continue

            self._cmd_map.append(Executor(fn))
        
    def call(self, name, *args):
        for exe in self._cmd_map:
            if exe.match_name(name):
                exe.try_invoke(*args)
                return

        raise ShconfigException(
                f"command not found {name}")
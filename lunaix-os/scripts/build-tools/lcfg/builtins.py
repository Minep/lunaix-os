from .api import contextual, builtin
from .lcnodes import LCFuncNode, LCTermNode, LCModuleNode
from lib.utils import join_path
import os

@contextual()
def v(env, caller, term):
    node = env.lookup_node(term.__name__)
    env.dependency().add(node, caller)
    
    return env.lookup_value(node.get_name())

@contextual(caller_type=[LCModuleNode])
def include(env, caller, file):
    fobj = caller.get_fo()
    path = os.path.dirname(fobj.filename())

    env.resolve_module(join_path(path, file))

@contextual("type", caller_type=[LCTermNode])
def term_type(env, caller, type):
    caller.set_type(type)

@contextual("add_to_collection", caller_type=[LCFuncNode])
def parent(env, caller, ref):
    sym = env.lookup_node(ref.__name__)

    caller.set_parent(sym)

@contextual(caller_type=[LCTermNode])
def default(env, caller, val):
    caller.set_default(val)

@contextual(caller_type=[LCTermNode])
def set_value(env, caller, val):
    caller.set_value(val)

@builtin()
def env(env, key, default=None):
    return os.getenv(key, default)
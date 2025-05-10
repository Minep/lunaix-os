import ast

from lib.utils  import SourceLogger, Schema
from .common     import NodeProperty, ConfigNodeError, NodeDependency
from .lazy       import LazyLookup
from .rewriter   import ConfigNodeASTRewriter

from .ast_validator import NodeValidator
from .rules import SyntaxRule

validator = NodeValidator(SyntaxRule())

class ConfigDecorator:
    Label       = Schema(ast.Constant)
    Readonly    = Schema(ast.Name, id="readonly")
    Hidden      = Schema(ast.Name, id="hidden")
    Flag        = Schema(ast.Name, id="flag")
    SetParent   = Schema(
                    ast.NamedExpr, 
                        target=Schema(ast.Name, id="parent"), 
                        value=Schema(ast.Name))
    SetProperty = Schema(
                    ast.NamedExpr,
                        target=Schema(ast.Name), 
                        value=Schema(ast.Name))


class ConfigNode:
    def __init__(self, env, filename, name):
        self.__props = {}
        self.__exec = None
        self._env = env
        self._filename = filename
        self._lazy_table = LazyLookup()

        self._name = name
        
        NodeProperty.Enabled[self]  = True
        NodeProperty.Hidden[self]   = False
        NodeProperty.Readonly[self] = False
        NodeProperty.Status[self]   = "Empty"

    def set_node_body(self, ast_nodes, rewriter = ConfigNodeASTRewriter):
        new_ast = ast.Module(ast_nodes, [])
        validator.validate(self, new_ast)
        
        new_ast = rewriter(self).rewrite(new_ast)
        NodeDependency.try_create(self)

        fn_name = f"__fn_{self._name}"
        args = ast.arguments([], [], None, [], [], None, [])
        module = ast.Module([
            ast.FunctionDef(fn_name, args, new_ast.body, []),
            ast.Assign(
                [ast.Name("_result", ctx=ast.Store())],
                ast.Call(ast.Name(fn_name,ctx=ast.Load()), [], [])
            )
        ], [])

        module = ast.fix_missing_locations(module)
        self.__exec = compile(module, self._filename, mode='exec')

    
    def apply_decorator(self, decor):
        if ConfigDecorator.Label == decor:
            NodeProperty.Label[self] = str(decor.value)
        
        elif ConfigDecorator.Readonly == decor:
            NodeProperty.Readonly[self] = True
        
        elif ConfigDecorator.Hidden == decor:
            NodeProperty.Hidden[self] = True  
        
        elif ConfigDecorator.SetParent == decor:
            NodeProperty.Parent[self] = decor.value.id

        elif ConfigDecorator.Flag == decor:
            NodeProperty.Hidden[self] = True
            NodeProperty.Readonly[self] = True

        elif ConfigDecorator.SetProperty == decor:
            self.set_property(decor.target.id, decor.value.id)

        else:
            fname = self._filename
            line  = decor.lineno
            col   = decor.col_offset
            msg   = f"unknown decorator: {ast.unparse(decor)}"
            msg   = SourceLogger.fmt_warning(fname, line, col, msg)
            print(msg)

    def update(self):
        self.update_status("Updating")
        
        try:
            env = self._env
            local = {}
            globl = {
                **env.context(),
                "__lzLut__": self._lazy_table
            }

            exec(self.__exec, globl, local)
            self.update_status("Latest")
        
            return local["_result"]
        
        except ConfigNodeError as e:
            self.update_status("Error")
            raise e
        
        except Exception as e:
            self.update_status("Error")
            raise self.config_error(e)
        
    def sanity_check(self):
        pass

    def get_property(self, key):
        return self.__props[key] if key in self.__props else None
    
    def set_property(self, key, value):
        if value is None:
            del self.__props[key]
        else:
            self.__props[key] = value

    def enabled(self):
        val = NodeProperty.Value[self]
        en  = NodeProperty.Enabled[self]
        parent = NodeProperty.Parent[self]

        if isinstance(val, bool):
            en = en and val
        
        if isinstance(parent, ConfigNode):
            en = en and parent.enabled()
        
        return en
    
    def config_error(self, *args):
        return ConfigNodeError(self, *args)
    
    def update_status(self, status):
        NodeProperty.Status[self] = status


class GroupNode(ConfigNode):
    def __init__(self, env, filename, name):
        super().__init__(env, filename, name)
        
        self._subnodes = {}

    def add_child(self, node):
        if node._name in self._subnodes:
            return
        
        NodeProperty.Parent[node] = self
        self._subnodes[node._name] = node


class TermNode(ConfigNode):
    def __init__(self, env, filename, name):
        super().__init__(env, filename, name)

    def update(self):
        result = super().update()
        
        if NodeProperty.Readonly[self]:
            NodeProperty.Value[self] = result
        
        elif NodeProperty.Value[self] is None:
            NodeProperty.Value[self] = result
        
        return result
    
    def sanity_check(self):
        val = NodeProperty.Value[self]
        value_typer = NodeProperty.Type[self]
        value_typer.ensure_type(self, val)
        
        super().sanity_check()


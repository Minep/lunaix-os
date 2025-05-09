import ast

from lib.utils  import Schema
from .lazy       import Lazy
from .common     import NodeProperty

class ConfigNodeASTRewriter(ast.NodeTransformer):
    Depend = Schema(
                ast.Call, 
                    func=Schema(ast.Name, id='require'),
                    args=[ast.expr])

    def __init__(self, cfg_node):
        super().__init__()

        self.__cfg_node = cfg_node

    def __gen_accessor(self, orig):
        key = Lazy.from_astn(self.__cfg_node, orig)
        if not key:
            return self.generic_visit(orig)
        
        return ast.Subscript(
            value=ast.Name("__lzLut__", ctx=ast.Load()),
            slice=ast.Constant(key),
            ctx=orig.ctx,
            lineno=orig.lineno,
            col_offset=orig.col_offset
        )
    
    def __gen_dependency(self, node):
        cfgn = self.__cfg_node
        dep_expr = NodeProperty.Dependency[cfgn]
        if dep_expr is None:
            NodeProperty.Dependency[cfgn] = node
            return

        if not isinstance(dep_expr, ast.expr):
            raise cfgn.config_error(
                f"invalid dependency state: {dep_expr}")
        
        dep_expr = ast.BoolOp(ast.And(), [dep_expr, node])
        NodeProperty.Dependency[cfgn] = dep_expr

    def visit_Attribute(self, node):
        return self.__gen_accessor(node)
    
    def visit_Expr(self, node):
        val = node.value
        
        if ConfigNodeASTRewriter.Depend != val:
            return self.generic_visit(node)
        
        # Process marker functions
        name = val.func.id
        if name == "require":
            self.__gen_dependency(val.args[0])
        else:
            return self.generic_visit(node)
        
        return None

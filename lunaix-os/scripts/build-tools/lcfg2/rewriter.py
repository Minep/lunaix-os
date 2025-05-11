import ast

from lib.utils  import Schema, SourceLogger
from .lazy       import Lazy
from .common     import NodeProperty, NodeInverseDependency

class RewriteRule:
    MaybeBuiltin = Schema(
                        ast.Call, 
                            func=Schema(ast.Name),
                            args=[ast.expr])

    WhenTogglerItem = Schema(
                            ast.Compare,
                                left=ast.Name,
                                ops=[Schema.Union(ast.Is, ast.IsNot)],
                                comparators=[ast.Constant])
    
    WhenToggler = Schema(
                    Schema.Union(
                        WhenTogglerItem,
                        Schema(ast.BoolOp, 
                            op=ast.And, 
                            values=Schema.List(WhenTogglerItem))))

class ConfigNodeASTRewriter(ast.NodeTransformer):
    

    def __init__(self, cfg_node):
        super().__init__()

        self.__cfg_node = cfg_node

        self.__when_epxr = None

    def __put_linkage(self, to_node, item):
        node = self.__cfg_node._env.get_node(to_node)
        link = NodeProperty.Linkage[node]
        
        if not link:
            link = NodeInverseDependency()
            NodeProperty.Linkage[node] = link
        
        link.add_linkage(self.__cfg_node._name, ast.unparse(item))

    def __subscript_accessor(self, name, ctx, token):
        return ast.Subscript(
            value=ast.Name("__lzLut__", ctx=ast.Load()),
            slice=ast.Constant(name),
            ctx=ctx,
        )

    def __gen_accessor(self, orig):
        key = Lazy.from_astn(self.__cfg_node, orig)
        if not key:
            return self.generic_visit(orig)
        
        return self.__subscript_accessor(key, orig.ctx, orig)
    
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

    def __gen_when_expr(self, node):
        and_list = []
        cfgn = self.__cfg_node

        if RewriteRule.WhenToggler != node:
            raise cfgn.config_error(
                f"when(...): invalid expression: {ast.unparse(node)}")

        if RewriteRule.WhenTogglerItem == node:
            and_list.append(node)
        else:
            and_list += node.values
        
        for i in range(len(and_list)):
            item = and_list[i]
            operator = item.ops[0]

            if RewriteRule.WhenTogglerItem != item:
                raise cfgn.config_error(
                        f"when(...): non-trivial subclause : {ast.unparse(node)}")
            
            name = Lazy.from_type(cfgn, Lazy.NodeValue, item.left.id)
            acc = self.__subscript_accessor(name, ast.Load(), node)
            
            self.__put_linkage(item.left.id, item)

            if isinstance(operator, ast.Is):
                operator = ast.Eq()
            else:
                operator = ast.NotEq()
            
            item.left = acc
            item.ops  = [operator]
            
        
        current = ast.BoolOp(
                        op=ast.And(), 
                        values=[ast.Constant(True), *and_list])
        
        expr = self.__when_epxr
        if expr:
            assert isinstance(expr, ast.expr)
            current = ast.BoolOp(op=ast.Or(), values=[expr, current])

        self.__when_epxr = current

    def visit_Attribute(self, node):
        return self.__gen_accessor(node)
    
    def visit_Expr(self, node):
        val = node.value
        
        if RewriteRule.MaybeBuiltin != val:
            return self.generic_visit(node)
        
        # Process marker functions
        name = val.func.id
        if name == "require":
            self.__gen_dependency(val.args[0])
        elif name == "when":
            self.__gen_when_expr(val.args[0])
        else:
            return self.generic_visit(node)
        
        return None
    
    def visit_Return(self, node):
        if self.__when_epxr:
            SourceLogger.warn(self.__cfg_node, node, 
                              "mixed use of `return` and `when` directive. "
                              "`when` have higher precedence than `return`. "
                              "consider remove `return` to avoid confusion")
            return None
        return self.generic_visit(node)
    
    def visit_Is(self, node):
        return ast.Eq()
    
    def rewrite(self, node):
        assert isinstance(node, ast.Module)
        node = self.visit(node)

        expr = self.__when_epxr
        if not expr:
            return node

        node.body.append(ast.Return(expr, lineno=0, col_offset=0))
        return node

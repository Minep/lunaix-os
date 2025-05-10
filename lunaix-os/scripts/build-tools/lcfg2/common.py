import ast

from lib.utils  import SourceLogger, Schema

class NodeProperty:
    class PropertyAccessor:
        def __init__(self, key):
            self.__key = key
        def __getitem__(self, node):
            return node.get_property(self.__key)
        def __setitem__(self, node, value):
            node.set_property(self.__key, value)
        def __delitem__(self, node):
            node.set_property(self.__key, None)

    Token       = PropertyAccessor("$token")
    Value       = PropertyAccessor("$value")
    Type        = PropertyAccessor("$type")
    Enabled     = PropertyAccessor("$enabled")
    Status      = PropertyAccessor("$status")
    Dependency  = PropertyAccessor("$depends")
    Linkage     = PropertyAccessor("$linkage")
    Hidden      = PropertyAccessor("hidden")
    Parent      = PropertyAccessor("parent")
    Label       = PropertyAccessor("label")
    Readonly    = PropertyAccessor("readonly")
    HelpText    = PropertyAccessor("help")

class ConfigNodeError(Exception):
    def __init__(self, node, *args):
        super().__init__(*args)

        self.__node = node
        self.__msg = " ".join([str(x) for x in args])

    def __str__(self):
        node = self.__node
        tok: ast.stmt = NodeProperty.Token[node]
        return (
            f"{node._filename}:{tok.lineno}:{tok.col_offset}:" +
            f" fatal error: {node._name}: {self.__msg}"
        )


class ValueTypeConstrain:
    TypeMap = {
        "str": str,
        "int": int,
        "bool": bool,
    }

    BinOpOr = Schema(ast.BinOp, op=ast.BitOr)

    def __init__(self, node, rettype):
        self.__node = node
        self.__raw = rettype
        
        if isinstance(rettype, ast.Expr):
            value = rettype.value
        else:
            value = rettype
        
        if isinstance(value, ast.Constant):
            self.schema = Schema(value.value)
        
        elif isinstance(value, ast.Name):
            self.schema = Schema(self.__parse_type(value.id))
        
        elif isinstance(value, ast.BinOp):
            unions = self.__parse_binop(value)
            self.schema = Schema(Schema.Union(*unions))
        
        else:
            raise Exception(
                f"unsupported type definition: {ast.unparse(rettype)}")

    def __parse_type(self, type):
        if type not in ValueTypeConstrain.TypeMap:
            raise Exception(f"unknown type: {type}")
        
        return ValueTypeConstrain.TypeMap[type]
    
    def __parse_binop(self, oproot):
        if isinstance(oproot, ast.Constant):
            return [oproot.value]
        
        if ValueTypeConstrain.BinOpOr != oproot:
            SourceLogger.warn(
                self.__node, self.__raw, 
                "only OR is allowed. Ignoring...")
            return []
        
        return self.__parse_binop(oproot.left) \
             + self.__parse_binop(oproot.right)
    
    def check_type(self, value):
        return self.schema.match(value)
    
    def ensure_type(self, node, val):
        if self.check_type(val):
           return

        raise node.config_error(
                f"unmatched type:",
                f"expect: '{self.schema}',",
                f"got: '{val}' ({type(val)})") 

class NodeDependency:
    class SimpleWalker(ast.NodeVisitor):
        def __init__(self, deps):
            super().__init__()
            self.__deps = deps
        
        def visit_Name(self, node):
            self.__deps._names.append(node.id)
    
    def __init__(self, nodes, expr: ast.expr):
        self._names = []
        self._expr  = ast.unparse(expr)

        expr = ast.fix_missing_locations(expr)
        self.__exec  = compile(ast.Expression(expr), "", mode='eval')

        NodeDependency.SimpleWalker(self).visit(expr)

    def evaluate(self, value_tables) -> bool:        
        return eval(self.__exec, value_tables)
    
    @staticmethod
    def try_create(node):
        expr = NodeProperty.Dependency[node]
        if not isinstance(expr, ast.expr):
            return
        
        dep = NodeDependency(node, expr)
        NodeProperty.Dependency[node] = dep


class NodeInverseDependency:
    def __init__(self):
        self.__map = {}

    def add_linkage(self, name, expr):
        if name not in self.__map:
            self.__map[name] = [expr]
        else:
            self.__map[name].append(expr)

    def linkages(self):
        return self.__map.items()
import ast

from lib.utils import Schema, ConfigASTVisitor, SourceLogger
from .common import NodeProperty

class TreeSanitiser(ConfigASTVisitor):
    DecoNative = Schema(ast.Name, id="native")
    DecoName   = Schema(ast.Name)
    DecoNameE  = Schema(ast.NamedExpr)
    DecoCall   = Schema(ast.Call)
    DecoConst  = Schema(ast.Constant)

    def __init__(self):
        super().__init__()
        self.logger = SourceLogger(self)

        # TODO
        self.deco_rules = {}

    def __sanitise_decorator(self, node: ast.FunctionDef):
        deco_map = {}

        for deco in node.decorator_list:
            name = ""
            if TreeSanitiser.DecoNative == deco:
                setattr(node, "__builtin", True)
                continue
            elif TreeSanitiser.DecoCall == deco:
                name = deco.func
            elif TreeSanitiser.DecoConst == deco:
                name = f"{deco.value}"
            elif TreeSanitiser.DecoName == deco:
                name = f"{deco.id}"
            elif TreeSanitiser.DecoNameE == deco:
                name = f"{ast.unparse(deco)}"
            else:
                self.logger.warn(deco, "invalid modifier type")
                continue

            deco_map[name] = deco

        node.decorator_list = [x for x in deco_map.values()]
        
        for desc, rule in self.deco_rules.items():
            if rule(deco_map):
                continue
            self.logger.warn(node, desc)

    def _visit_fndef(self, node):
        self.__sanitise_decorator(node)
        super()._visit_fndef(node)

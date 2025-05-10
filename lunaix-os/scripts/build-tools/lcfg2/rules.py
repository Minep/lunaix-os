import ast

from .ast_validator import RuleCollection, rule
from lib.utils import Schema

class SyntaxRule(RuleCollection):
    NodeAssigment = Schema(ast.Subscript, 
                            value=Schema(ast.Name, id='__lzLut__'), 
                            ctx=ast.Store)
    TrivialValue = Schema(Schema.Union(
        ast.Constant, 
        ast.Name
    ))

    BoolOperators = Schema(Schema.Union(ast.Or, ast.And))
    
    TrivialTest    = Schema(ast.Compare, 
                          left=TrivialValue, 
                          ops=[Schema.Union(ast.Eq)],
                          comparators=[ast.Constant])
    
    InlineIf       = Schema(ast.IfExp, 
                            test=Schema.Union(TrivialTest, TrivialValue), 
                            body=TrivialValue, 
                            orelse=TrivialValue)
    
    TrivialLogic   = Schema(ast.BoolOp, 
                            op=BoolOperators, 
                            values=Schema.List(
                                Schema.Union(TrivialTest, ast.Name)
                            ))
    
    TrivialReturn  = Schema(Schema.Union(
        TrivialValue,
        ast.JoinedStr
    ))

    def __init__(self):
        super().__init__()
    
    @rule(ast.If, None, "dynamic-logic")
    def __dynamic_logic(self, reducer, node):
        """
        Conditional branching could interfering dependency resolving
        """
        return False
    
    @rule(ast.While, None, "while-loop")
    def __while_loop(self, reducer, node):
        """
        loop construct may impact with readability.
        """
        return False
    
    @rule(ast.For, None, "for-loop")
    def __for_loop(self, reducer, node):
        """
        loop construct may impact with readability.
        """
        return False
    
    @rule(ast.ClassDef, None, "class-def")
    def __class_definition(self, reducer, node):
        """
        use of custom class is not recommended
        """
        return False
    
    @rule(ast.Dict, None, "complex-struct")
    def __complex_datastruct(self, reducer, node):
        """
        use of complex data structure is not recommended
        """
        return False
    
    @rule(ast.Subscript, NodeAssigment, "side-effect-option")
    def __side_effect(self, reducer, node):
        """
        Option modifying other options dynamically unpredictable behaviour
        """
        return False
    
    @rule(ast.Return, None, "non-trivial-value")
    def __nontrivial_return(self, reducer, node):
        """
        Option default should be kept as constant or simple membership check
        """
        return SyntaxRule.TrivialReturn == node.value 
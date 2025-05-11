import ast

from .ast_validator import RuleCollection, rule
from lib.utils import Schema

class SyntaxRule(RuleCollection):
    NodeAssigment = Schema(ast.Subscript, 
                            value=Schema(ast.Name, id='__lzLut__'), 
                            ctx=ast.Store)

    TrivialReturn  = Schema(Schema.Union(
        ast.Constant,
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
        Use of non-trivial value as default value
        """
        return SyntaxRule.TrivialReturn == node.value 
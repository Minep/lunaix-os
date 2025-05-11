import ast
import inspect
import textwrap

from typing import Callable
from lib.utils import Schema, ConfigASTVisitor, SourceLogger
from .common import Schema, ConfigNodeError

class Rule:
    def __init__(self, t, v, name, fn):
        self.type = t
        self.__name = name
        self.__var = v
        self.__fn = fn
        self.__help_msg = inspect.getdoc(fn)
        self.__help_msg = textwrap.dedent(self.__help_msg.strip())

    def match_variant(self, astn):
        if not self.__var:
            return True
        return self.__var.match(astn)
    
    def invoke(self, reducer, node):
        if self.__fn(reducer._rules, reducer, node):
           return

        SourceLogger.warn(reducer._cfgn, node, 
                          f"rule violation: {self.__name}: {self.__help_msg}")
        # raise ConfigNodeError(reducer._cfgn, 
        #         f"rule failed: {self.__name}: {self.__help_msg}")
 

def rule(ast_type: type, variant: Schema, name: str):
    def __rule(fn: Callable):
        return Rule(ast_type, variant, name, fn)
    return __rule

class RuleCollection:
    def __init__(self):
        self.__rules = {}

        members = inspect.getmembers(self, lambda p: isinstance(p, Rule))
        for _, rule in members:
            t = rule.type
            if rule.type not in self.__rules:
                self.__rules[t] = [rule]
            else:
                self.__rules[t].append(rule)
    
    def execute(self, reducer, node):
        rules = self.__rules.get(type(node))
        if not rules:
            return
        
        for rule in rules:
            if not rule.match_variant(node):
                continue
            rule.invoke(reducer, node)

class NodeValidator(ast.NodeTransformer):
    def __init__(self, all_rules):
        super().__init__()
        self._rules = all_rules

    def validate(self, cfgn, astn):
        self._cfgn = cfgn
        self.visit(astn)

    def visit(self, node):
        self._rules.execute(self, node)
        return super().visit(node)

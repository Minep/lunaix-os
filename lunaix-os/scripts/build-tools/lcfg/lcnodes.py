from .api import (
    ConfigLoadException,
    ConfigTypeCheckError
)

from .utils import (
    extract_decorators, 
    to_displayable
)

import ast
from abc import abstractmethod



class LCNode:
    def __init__(self, fo, astn) -> None:
        self._fo = fo
        self._env = fo.env()
        self._astn = self.prune_astn(astn)
        self._co = self.compile_astn()
        self._parent = None

        self._env.register_node(self)
    
    def prune_astn(self, astn):
        return astn

    def compile_astn(self):
        return self._fo.compile_astns(self._astn)

    def get_co(self):
        return self._co
    
    def get_fo(self):
        return self._fo
    
    def set_parent(self, new_parent):
        self._parent = new_parent

    def parent(self):
        return self._parent
    
    def add_child(self, node):
        pass

    def remove_child(self, node):
        pass
    
    def get_name(self):
        return f"LCNode: {self.__hash__()}"
    
    @abstractmethod
    def evaluate(self):
        pass

    def render(self, rctx):
        pass

    def deserialise(self, dict):
        pass

    def serialise(self, dict):
        pass



class LCModuleNode(LCNode):
    def __init__(self, fo, astn: ast.Module):
        self.__nodes = {}

        super().__init__(fo, astn)

    def get_name(self):
        return f"file: {self._fo.filename()}"

    def prune_astn(self, astn: ast.Module):
        general_exprs = []

        for b in astn.body:
            if not isinstance(b, ast.FunctionDef):
                general_exprs.append(b)
                continue
            
            node = LCFuncNode.construct(self._fo, b)
            node.set_parent(self)

            self.add_child(node)

        return general_exprs
    
    def evaluate(self):
        with self._env.eval_context(self) as evalc:
            ls = list(self.__nodes.values())          
            for node in ls:
                node.evaluate()

    def add_child(self, node):
        self.__nodes[node] = node

    def remove_child(self, node):
        if node in self.__nodes:
            del self.__nodes[node]

    def deserialise(self, dict):
        for node in self.__nodes:
            node.deserialise(dict)

    def serialise(self, dict):
        for node in self.__nodes:
            node.serialise(dict)

class LCFuncNode(LCNode):
    def __init__(self, fo, astn) -> None:
        self._decors = {}
        self._name = None
        self._help = ""
        self._display_name = None
        self._enabled = True

        super().__init__(fo, astn)

    def prune_astn(self, astn: ast.FunctionDef):
        self._name = astn.name
        self._display_name = to_displayable(self._name)
        
        maybe_doc = astn.body[0]
        if isinstance(maybe_doc, ast.Constant):
            self._help = maybe_doc.value
        
        decors = extract_decorators(astn)
        for name, args, kwargs in decors:
            self._decors[name] = (args, kwargs)

        (args, _) = self._decors[self.mapped_name()]
        if args:
            self._display_name = args[0]
        
        astn.decorator_list.clear()
        return astn
    
    def get_name(self):
        return self._name
    
    def get_display_name(self):
        return self._display_name
    
    def enabled(self):
        if isinstance(self._parent, LCFuncNode):
            return self._enabled and self._parent.enabled()
        return self._enabled

    def help_prompt(self):
        return self._help
    
    def evaluate(self):
        with self._env.eval_context(self) as evalc:
            result = evalc.evaluate()
            self._enabled = True if result is None else result
            
    @staticmethod
    def mapped_name(self):
        return None
    
    @staticmethod
    def construct(fo, astn: ast.FunctionDef):
        nodes = [
            LCCollectionNode,
            LCGroupNode,
            LCTermNode
        ]

        for node in nodes:
            if extract_decorators(astn, node.mapped_name(), True):
                return node(fo, astn)
        
        raise ConfigLoadException(
                f"unknown type for astn type: {type(astn)}")

    def set_parent(self, new_parent):
        if self._parent:
            self._parent.remove_child(self)
        
        new_parent.add_child(self)
        super().set_parent(new_parent)


class LCTermNode(LCFuncNode):
    def __init__(self, fo, astn) -> None:
        self._value = None
        self._default = None
        self._type = None
        self._rdonly = False

        super().__init__(fo, astn)

    @staticmethod
    def mapped_name():
        return "Term"
    
    def prune_astn(self, astn: ast.FunctionDef):
        astn = super().prune_astn(astn)

        self._rdonly = "ReadOnly" in self._decors

        return astn

    def set_type(self, type_def):
        self._type = self._env.type_factory().create(type_def)

    def __assert_type(self, val):
        if not self._type:
            raise ConfigLoadException(
                    f"config: {self._name} must be typed", self)
        
        if self._type.check(val):
           return

        raise ConfigTypeCheckError(
                f"value: {val} does not match type {self._type}", self)

    def set_value(self, val):
        if self._rdonly:
            return
        
        self.__assert_type(val)
        self._value = val
        self.__update_value()
        self._env.dependency().cascade(self)

    def set_default(self, val):
        self.__assert_type(val)
        self._default = val

        if self._rdonly:
            self._value = val

    def get_value(self):
        return self._value
    
    def evaluate(self):
        super().evaluate()
        self.__update_value()

    def read_only(self):
        return self._rdonly

    def render(self, rctx):
        rctx.add_field(self._display_name, self)

    def serialise(self, dict):
        s_val = self._type.serialise(self._value)
        dict[self._name] = s_val

    def deserialise(self, dict):
        if self._name not in dict:
            return
        
        s_val = dict[self._name]
        v = self._type.deserialise(s_val)
        self.__assert_type(v)
        self._value = v


    def __update_value(self):
        v = self._value

        if not self.enabled():
            self._env.update_value(self._name, None)
            return

        if v is None:
            v = self._default

        if v is None and not self._type.allow_none():
            raise ConfigLoadException(
                    f"config: {self._name} must have a value", self)
        
        self._value = v
        self._env.update_value(self._name, v)
    



class LCGroupNode(LCFuncNode):
    def __init__(self, fo, astn) -> None:
        self._children = {}

        super().__init__(fo, astn)

    @staticmethod
    def mapped_name():
        return "Group"

    def prune_astn(self, astn: ast.FunctionDef):
        astn = super().prune_astn(astn)

        other_exprs = []
        for expr in astn.body:
            if not isinstance(expr, ast.FunctionDef):
                other_exprs.append(expr)
                continue

            node = LCFuncNode.construct(self._fo, expr)
            node.set_parent(self)
            self._children[node] = node

        if not other_exprs:
            other_exprs.append(
                ast.Pass(
                    lineno=astn.lineno + 1, 
                    col_offset=astn.col_offset
                )
            )

        astn.body = other_exprs
        return astn

    def evaluate(self):
        old_enable = super().enabled()
        super().evaluate()

        new_enabled = super().enabled()
        if not new_enabled and old_enable == new_enabled:
            return

        with self._env.eval_context(self) as evalc:
            children = list(self._children.values())
            for child in children:
                child.evaluate()
        
    def render(self, rctx):
        for child in self._children.values():
            child.render(rctx)

    def serialise(self, dict):
        sub_dict = {}
        for child in self._children.values():
            child.serialise(sub_dict)
        
        dict[self._name] = sub_dict

    def deserialise(self, dict):
        if self._name not in dict:
            return
        
        sub_dict = dict[self._name]
        for child in self._children.values():
            child.deserialise(sub_dict)
    
    def add_child(self, node):
        self._children[node] = node

    def remove_child(self, node):
        if node in self._children:
            del self._children[node]


class LCCollectionNode(LCGroupNode):
    def __init__(self, fo, astn) -> None:
        super().__init__(fo, astn)

    @staticmethod
    def mapped_name():
        return "Collection"
    
    def render(self, rctx):
        rctx.add_expandable(
            self._display_name, 
            lambda _ctx: 
                super().render(_ctx)
        )
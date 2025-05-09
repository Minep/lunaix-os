import os, ast
from pathlib import Path

def join_path(stem, path):
    if os.path.isabs(path):
        return path
    return os.path.join(stem, path)


class Schema:
    class Any:
        def __init__(self):
            pass

        def __str__(self):
            return "Any"

    class Union:
        def __init__(self, *args):
            self.union = [*args]

        def __str__(self):
            strs = [Schema.get_type_str(t) for t in self.union]
            return f"{' | '.join(strs)}"

    def __init__(self, type, **kwargs):
        self.__type = type
        self.__fields = kwargs

    def __match_list(self, actual, expect):
        if len(actual) != len(expect):
            return False
        
        for a, b in zip(actual, expect):
            if not self.__match(a, b):
                return False
            
        return True
    
    def __match_union(self, actual, union):
        for s in union.union:
            if self.__match(actual, s):
                return True
        return False

    def __match(self, val, scheme):
        if isinstance(scheme, Schema.Any):
            return True
        
        if isinstance(scheme, Schema):
            return scheme.match(val)
        
        if isinstance(scheme, list) and isinstance(val, list):
            return self.__match_list(val, scheme)
        
        if isinstance(scheme, Schema.Union):
            return self.__match_union(val, scheme)
        
        if not isinstance(scheme, type):
            return scheme == val
        
        return isinstance(val, scheme)
    
    def match(self, instance):
        if not self.__match(instance, self.__type):
            return False

        for field, t in self.__fields.items():
            if not hasattr(instance, field):
                return False
            
            field_val = getattr(instance, field)

            if not self.__match(field_val, t):
                return False

        return True

    def __eq__(self, value):
        if isinstance(value, Schema):
            return super().__eq__(value)
        return self.match(value)
    
    def __ne__(self, value):
        return not self.__eq__(value)
    
    def __str__(self):
        fields = ""
        if len(self.__fields) > 0:
            fields = ", ".join([
                f"{name} :: {Schema.get_type_str(t)}" 
                for name, t in self.__fields.items()])
            fields = "{%s}"%(fields)
        
        return f"{Schema.get_type_str(self.__type)} {fields}"
    
    def __repr__(self):
        return self.__str__()
    
    @staticmethod
    def get_type_str(maybe_type):
        if isinstance(maybe_type, type):
            return maybe_type.__name__
        if isinstance(maybe_type, str):
            return f'"{maybe_type}"'
        return str(maybe_type)

class SourceLogger:
    def __init__(self, visitor):
        self.__visitor = visitor

    def warn(self, node, fmt, *args):
        fname = self.__visitor.current_file()
        line  = node.lineno
        coln  = node.col_offset

        print(SourceLogger.fmt_warning(fname, line, coln, fmt%args))

    @staticmethod
    def fmt_message(fname, line, col, level, msg):
        return "%s:%d:%d: %s: %s"%(fname, line, col, level, msg)

    @staticmethod
    def fmt_warning(fname, line, col, msg):
        return SourceLogger.fmt_message(fname, line, col, "warning", msg)
    
    @staticmethod
    def fmt_info(fname, line, col, msg):
        return SourceLogger.fmt_message(fname, line, col, "info", msg)
    
    @staticmethod
    def fmt_error(fname, line, col, msg):
        return SourceLogger.fmt_message(fname, line, col, "error", msg)
    
    @staticmethod
    def log(level, node, token, msg):
        fname = node._filename
        line  = token.lineno
        col   = token.col_offset
        
        print( SourceLogger.fmt_message(fname, line, col, level, msg))
        print(f"        at... {ast.unparse(token)}")
    
    @staticmethod
    def warn(node, token, msg):
        return SourceLogger.log("warning", node, token, msg)
    
class ConfigAST:
    ConfigImport = Schema(ast.ImportFrom, level=1)

    class EnterFileMarker:
        def __init__(self, filename = None):
            self.name = filename

    class LeaveFileMarker:
        def __init__(self):
            pass

    def __init__(self, root_file, cfg_name = "LConfig"):
        self.__tree = ast.Module([])
        self.__cfg_name = cfg_name
        self.__rootfile = Path(root_file)

        self.__load(self.__rootfile)

    def __load(self, file: Path):
        parent = file.parent
        
        with file.open('r') as f:
            nodes = ast.parse(f.read()).body
        
        relfile = str(file.relative_to(self.__rootfile.parent))
        marker  = ConfigAST.EnterFileMarker(relfile)
        self.__append_tree(marker)
        
        for node in nodes:
            if ConfigAST.ConfigImport != node:
                self.__append_tree(node)
                continue
            
            module = node.module
            module = "" if not module else module
            path = parent.joinpath(*module.split('.'))
            for alia in node.names:
                p = path / alia.name / self.__cfg_name
                self.__load(p)
        
        self.__append_tree(ConfigAST.LeaveFileMarker())
    
    def __append_tree(self, node):
        self.__tree.body.append(node)    

    def visit(self, visitor):
        visitor.visit(self.__tree)

class ConfigASTVisitor:
    def __init__(self):
        self.__markers = []

    def _visit_fndef(self, node : ast.FunctionDef):
        self._visit_subtree(node)

    def _visit_leaf(self, node):
        pass

    def _visit_subtree(self, node):
        for n in node.body:
            self.visit(n)

    def _visit_expr(self, node : ast.Expr):
        pass

    def current_file(self):
        if len(self.__markers) == 0:
            return "<root>"
        return self.__markers[-1].name

    def visit(self, node):
        if isinstance(node, ConfigAST.EnterFileMarker):
            self.__markers.append(node)
            return
        
        if isinstance(node, ConfigAST.LeaveFileMarker):
            self.__markers.pop()
            return

        if isinstance(node, ast.FunctionDef):
            self._visit_fndef(node)
        
        elif isinstance(node, ast.Expr):
            self._visit_expr(node)
        
        elif hasattr(node, "body"):
            self._visit_subtree(node)
        
        else:
            self._visit_leaf(node)

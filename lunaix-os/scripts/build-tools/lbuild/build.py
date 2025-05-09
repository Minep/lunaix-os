import ast
from pathlib    import Path
from .common    import DirectoryTracker
from lib.utils  import ConfigAST, Schema

class LBuildImporter(ast.NodeTransformer):
    ConfigImportFn = Schema(
                        ast.Call, 
                        func=Schema(ast.Name, id="import_"),
                        args=[ast.Constant])
    
    ScopedAccess = Schema(
                        ast.Attribute,
                        value=ast.Name)
    
    def __init__(self, env, file):
        super().__init__()
        self.__parent = file.parent
        self.__env = env

        with file.open('r') as f:
            subtree = ast.parse(f.read())
        
        self.__tree = [
            DirectoryTracker.emit_enter(self.__parent),
            *self.visit(subtree).body,
            DirectoryTracker.emit_leave()
        ]
    
    def tree(self):
        return self.__tree
    
    def __gen_import_subtree(self, relpath, variants):    
        block = []
        for var in variants:
            p = relpath/ var / "LBuild"
            block += LBuildImporter(self.__env, p).tree()
        
        return ast.If(ast.Constant(True), block, [])

    def visit_ImportFrom(self, node):
        if ConfigAST.ConfigImport != node:
            return node
        
        module = node.module
        module = "" if not module else module
        subpath = Path(*module.split('.'))
        
        p = self.__parent / subpath
        return self.__gen_import_subtree(p, [x.name for x in node.names])
    
    def visit_Attribute(self, node):
        if LBuildImporter.ScopedAccess != node:
            return self.generic_visit(node)
        
        scope = node.value.id
        subscope = node.attr

        if scope not in self.__env.scopes:
            return self.generic_visit(node)
        
        provider = self.__env.scopes[scope]
        
        return ast.Subscript(
            value=ast.Name(provider.context_name(), ctx=node.value.ctx),
            slice=ast.Constant(subscope),
            ctx = node.ctx
        )
    
    def visit_Expr(self, node):
        val = node.value
        if LBuildImporter.ConfigImportFn == val:
            name = val.args[0].value
            return self.__gen_import_subtree(self.__parent, [name])
        
        return self.generic_visit(node)

class BuildEnvironment:
    def __init__(self):
        self.scopes = {}
    
    def load(self, rootfile):
        self.__exec = self.__load(Path(rootfile))

    def register_scope(self, scope):
        if scope.name in self.scopes:
            raise Exception(f"{scope.name} already exists")
        self.scopes[scope.name] = scope

    def __load(self, rootfile):
        module = ast.Module(
                    LBuildImporter(self, rootfile).tree(), 
                    type_ignores=[])

        module = ast.fix_missing_locations(module)
        return compile(module, rootfile, 'exec')
    
    def update(self):
        g = {
            "__LBUILD__": True,
        }

        for scope in self.scopes.values():
            scope.reset()
            g[scope.context_name()] = scope

        DirectoryTracker.bind(g)
        exec(self.__exec, g)
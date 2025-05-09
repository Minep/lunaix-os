import ast
from pathlib import Path

class DirectoryTracker:
    def __init__(self):
        self.__stack = []

    def push(self, dir):
        self.__stack.append(Path(dir))

    def pop(self):
        self.__stack.pop()
    
    def active_relative(self):
        root = self.__stack[0]
        return self.__stack[-1].relative_to(root)

    @staticmethod
    def context_name():
        return "__FILESTACK__"
    
    @staticmethod
    def get(context):
        return context[DirectoryTracker.context_name()]
    
    @staticmethod
    def bind(context):
        name = DirectoryTracker.context_name()
        context[name] = DirectoryTracker()

    @staticmethod
    def emit_enter(dir):
        name = DirectoryTracker.context_name()
        return ast.Expr(
                    ast.Call(
                        func=ast.Attribute(
                            value=ast.Name(name, ctx=ast.Load()),
                            attr="push",
                            ctx=ast.Load()
                        ),
                        args=[
                            ast.Constant(str(dir))
                        ],
                        keywords=[]))

    @staticmethod
    def emit_leave():
        name = DirectoryTracker.context_name()
        return ast.Expr(
                    ast.Call(
                        func=ast.Attribute(
                            value=ast.Name(name, ctx=ast.Load()),
                            attr="pop",
                            ctx=ast.Load()
                        ),
                        args=[],
                        keywords=[]))

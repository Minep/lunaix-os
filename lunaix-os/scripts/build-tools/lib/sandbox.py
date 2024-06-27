import traceback, sys

class InterpreterException(Exception):
    pass

class Sandbox:
    def __init__(self, symbols) -> None:
        self.__syms = globals()
        self.__syms.update(symbols)

    def execute(self, file):
        with open(file) as f:
            return self.executes(f.read(), file)
    
    def executes(self, str, file=""):
        try:
            local_ctx = {}
            glb_ctx = self.__syms.copy()
            exec(str, glb_ctx, local_ctx)
            return local_ctx
        except SyntaxError as err:
            error_class = err.__class__.__name__
            detail = err.args[0]
            line_number = err.lineno
        except Exception as err:
            error_class = err.__class__.__name__
            detail = err.args[0]
            cl, exc, tb = sys.exc_info()
            line_number = traceback.extract_tb(tb)[1][1]

        print(f"LunaBuild failed: {error_class} at ./{file}:{line_number}, {detail}")
        raise InterpreterException("load error")
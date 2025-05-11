import inspect
from lib.utils import Schema
from .common import DirectoryTracker

class ScopeProvider:
    Enumerables = Schema(Schema.Union(list, tuple))
    Primitives = Schema(Schema.Union(str, int, bool))
    
    def __init__(self, name):
        super().__init__()
        self.name = name
        self.__accessors = {}

    def __add_accessor(self, accessor, type):
        acc = accessor
        if isinstance(accessor, str):
            acc = type(accessor)
        self.__accessors[acc.name] = acc

    def subscope(self, accessor):
        self.__add_accessor(accessor, ScopeAccessor)

    def file_subscope(self, accessor):
        self.__add_accessor(accessor, FileScopeAccessor)

    def accessors(self):
        return self.__accessors.values()
    
    def reset(self):
        for v in self.__accessors.values():
            v.reset()

    def context_name(self):
        return f"__SC{self.name}"

    def __getitem__(self, name):
        if name not in self.__accessors:
            return None
        return self.__accessors[name]
    
    def __setitem__(self, name, val):
        pass

class ScopeAccessor:
    def __init__(self, name):
        super().__init__()
        self.name = name
        self.__values = []
        self._active_context = None

    def values(self):
        return self.__values
    
    def reset(self):
        self.__values.clear()

    def add(self, x):
        self.__load_build_context()

        if ScopeProvider.Enumerables == x:
            for v in x:
                self.add(v)
            return
        
        if ScopeProvider.Primitives == x:
            self.put_value(x)
            return
        
        raise Exception(f"invalid value '{x}' of type ('{type(x).__name__}')")
    
    def put_value(self, x):
        self.__values.append(x)

    def __load_build_context(self):
        self._active_context = None
        for f in inspect.stack():
            if "__LBUILD__" not in f.frame.f_globals:
                continue
            self._active_context = f.frame.f_globals
        
        if not self._active_context:
            raise Exception(
                f"unable to modify '{self.name}' outside build context)")

    def __iadd__(self, other):
        self.add(other)

class FileScopeAccessor(ScopeAccessor):
    def __init__(self, name):
        super().__init__(name)

    def put_value(self, x):
        tracker = DirectoryTracker.get(self._active_context)
        x = str(tracker.active_relative() / x)
        return super().put_value(x)


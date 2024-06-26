from abc import abstractmethod

class ConfigLoadBaseException(Exception):
    def __init__(self, msg, node, inner=None) -> None:
        super().__init__(msg)
        
        self.__msg = msg
        self.__node = node
        self.__inner = inner

    def __str__(self) -> str:
        return super().__str__()
    
class ConfigLoadException(ConfigLoadBaseException):
    def __init__(self, msg, node = None, inner=None) -> None:
        super().__init__(msg, node, inner)

class ConfigTypeCheckError(ConfigLoadBaseException):
    def __init__(self, msg, node, inner=None) -> None:
        super().__init__(msg, node, inner)

class ConfigIOProvider:
    def __init__(self) -> None:
        pass

    @abstractmethod
    def export(self, env, config_dict):
        pass
    
    @abstractmethod
    def save(self, env, config_dict):
        pass

    @abstractmethod
    def load(self, env):
        pass


class TypeProviderBase:
    def __init__(self, type) -> None:
        self._type = type

    @abstractmethod
    def check(self, val):
        return True
    
    @abstractmethod
    def serialise(self, val):
        pass

    @abstractmethod
    def deserialise(self, val):
        pass

    @abstractmethod
    def parse_input(self, input_str):
        pass

    @abstractmethod
    def to_input(self, val):
        pass

    def allow_none(self):
        return False
    
    @staticmethod
    def typedef_matched(typedef):
        return False

class RenderContext:
    def __init__(self) -> None:
        pass

    @abstractmethod
    def add_expandable(self, label, on_expand_cb):
        pass

    @abstractmethod
    def add_field(self, label, node):
        pass

class InteractiveRenderer(RenderContext):
    def __init__(self) -> None:
        super().__init__()

    @abstractmethod
    def render_loop(self):
        pass


class LConfigBuiltin:
    def __init__(self, f, is_contextual, rename=None, caller_type=[]):
        self.name = f.__name__ if not rename else rename
        self.__f = f
        self.__contextual = is_contextual
        self.__caller_type = caller_type

    def __call__(self, env, *args, **kwds):
        if not self.__contextual:
            return self.__f(env, *args, **kwds)
        
        caller = env.callframe_at(0)
        f_name = self.__f.__name__

        if not caller:
            raise ConfigLoadException(
                f"contextual function '{f_name}' can not be called contextless",
                None
            )
        
        if self.__caller_type and not any([isinstance(caller, x) for x in self.__caller_type]):
            raise ConfigLoadException(
                f"caller of '{f_name}' ({caller}) must have type of any {self.__caller_type}",
                caller
            )
    
        return self.__f(env, caller, *args, **kwds)
    
def contextual(name=None, caller_type=[]):
    def wrapper(func):
        return LConfigBuiltin(func, True, name, caller_type)
    return wrapper

def builtin(name=None):
    def wrapper(func):
        return LConfigBuiltin(func, False, name)
    return wrapper

from enum import Enum

class PrettyPrintable:
    def __init__(self) -> None:
        pass

    def print_simple(self, pp, *args):
        raise NotImplementedError()
    
    def print_abstract(self, pp, *args):
        raise NotImplementedError()
    
    def print_detailed(self, pp, *args):
        raise NotImplementedError()
    
class PrintMode(Enum):
    Simple = 0
    Detailed = 1
    Abstract = 2

class TypeConverter(Enum):
    Identity = lambda v: v
    CString = lambda v: v.string()

class MyPrettyPrinter:
    INDENT = 3
    def __init__(self, level = 0, prefix='') -> None:
        self.__level = level
        self.__padding = " " * MyPrettyPrinter.INDENT * level
        self.__prefix = prefix

    def set_prefix(self, prefix):
        self.__prefix = prefix
        return self
    
    def clear_prefix(self):
        self.__prefix = ''
        return self

    def next_level(self, indent_inc = 1):
        return MyPrettyPrinter(indent_inc + self.__level, self.__prefix)
    
    def print(self, *vals, indent=0, mode=PrintMode.Simple):
        val = '' if len(vals) == 0 else vals[0]
        if isinstance(val, PrettyPrintable):
            pp = self
            if indent > 0:
                pp = self.next_level(indent)
            [ val.print_simple, 
              val.print_detailed, 
              val.print_abstract ][mode.value](pp)
        else:
            new_id = " " * indent
            print(f"{self.__padding}{new_id}", f"{self.__prefix}{val}", *vals[1:])
        
        return self

    def printf(self, fmt, *args, indent=0):
        assert isinstance(fmt, str)
        self.print(fmt%args, indent=indent)
        return self
    
    def printfa(self, fmt, *args, indent=0):
        assert isinstance(fmt, str)
        self.print(fmt.format(*args), indent=indent)
        return self


    def print_field(self, obj, field, fmt=None, val=None, cast=TypeConverter.Identity):
        val = obj[field] if val is None else val
        val = cast(val)

        if fmt is None:
            self.printf("%s: %s", field, val)
        else:
            self.printf("%s: %s", field, fmt%(val))
        return self
import gdb
from enum import StrEnum

class SymbolDomain(StrEnum):
    DEBUG = "debug"

class LunaixSymbols:
    class SymbolAccesser:
        def __init__(self, sym) -> None:
            self.sym = f"({sym})"
            self.__sym = gdb.parse_and_eval(self.sym)
        
        def deref_and_access(self, members):
            return gdb.parse_and_eval(f"{self.sym}->{members}")
        
        def access(self, members):
            return gdb.parse_and_eval(f"{self.sym}.{members}")
        
        def __getitem__(self, index):
            return self.__sym[index]
        
        def value(self):
            return self.__sym

    @staticmethod
    def exported(domain, namespace, sym_name):
        name = f"*__SYMEXPORT_Z{domain.value}_N{namespace}_S{sym_name}"
        return LunaixSymbols.SymbolAccesser(name)
    
    @staticmethod
    def debug_sym(namespace, sym_name):
        name = f"*__SYMEXPORT_Z{SymbolDomain.DEBUG}_N{namespace}_S{sym_name}"
        return LunaixSymbols.SymbolAccesser(name)
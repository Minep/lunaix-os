import gdb
from enum import StrEnum

class SymbolDomain(StrEnum):
    DEBUG = "debug"

class LunaixSymbols:
    class SymbolAccesser:
        def __init__(self, sym) -> None:
            self.sym = f"({sym})"
        
        def deref_and_access(self, members):
            return gdb.parse_and_eval(f"{self.sym}->{members}")
        
        def access(self, members):
            return gdb.parse_and_eval(f"{self.sym}.{members}")

    @staticmethod
    def exported(domain, sym_name):
        name = f"*__SYMEXPORT_Z{domain.value}_{sym_name}"
        return LunaixSymbols.SymbolAccesser(name)
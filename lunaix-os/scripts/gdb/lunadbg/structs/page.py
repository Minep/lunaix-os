from gdb import Type, Value, lookup_type
from . import KernelStruct

class PageStruct(KernelStruct):
    def __init__(self, gdb_inferior: Value) -> None:
        super().__init__(gdb_inferior, PageStruct)
        self.ref = self._kstruct["ref_counts"]
        self.attr = self._kstruct["attr"]

    @staticmethod
    def get_type() -> Type:
        return lookup_type("struct pp_struct").pointer()
    

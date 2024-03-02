from gdb import Type, Value, lookup_type
from . import KernelStruct

class PMem(KernelStruct):
    def __init__(self, gdb_inferior: Value) -> None:
        super().__init__(gdb_inferior, PMem)

    @staticmethod
    def get_type():
        return lookup_type("struct pmem").pointer()
    
    def pplist(self):
        return self._kstruct["pplist"]
    
    def list_len(self):
        return self._kstruct["list_len"]
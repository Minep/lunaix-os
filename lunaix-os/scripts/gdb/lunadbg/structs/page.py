from gdb import Type, Value, lookup_type
from . import KernelStruct

class PageStruct(KernelStruct):
    def __init__(self, gdb_inferior: Value) -> None:
        super().__init__(gdb_inferior, PageStruct)
        self.ref = self._kstruct["refs"]
        self.type = self._kstruct["type"]
        self.flags = self._kstruct["flags"]
        self.order = self._kstruct["order"]
        self.pool = self._kstruct["pool"]
        self.companion = self._kstruct["companion"]

    def uninitialized(self):
        return not (self.flags & 0b10)
    
    def reserved(self):
        return (not self.uninitialized() 
                and self.type == 0b1000
                and self.ref  == 0xf0f0f0f0)
    
    def busy(self):
        return (not self.uninitialized()
                and self.type != 0b1000
                and self.ref > 0)

    def lead_page(self):
        return self.companion == 0 and not self.uninitialized()

    @staticmethod
    def get_type() -> Type:
        return lookup_type("struct ppage").pointer()
    

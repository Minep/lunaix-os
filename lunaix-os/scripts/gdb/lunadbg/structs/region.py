from gdb import Type, Value, lookup_type
from . import KernelStruct
from ..utils import get_dnode_path

class MemRegion(KernelStruct):
    def __init__(self, gdb_inferior: Value) -> None:
        super().__init__(gdb_inferior, MemRegion)
        self.__attr = self._kstruct["attr"]

    @staticmethod
    def get_type() -> Type:
        return lookup_type("struct mm_region").pointer()
    
    def print_abstract(self, pp, *args):
        self.print_detailed(pp, *args)
    
    def print_simple(self, pp, *args):
        self.print_detailed(pp, *args)
    
    def print_detailed(self, pp, *args):
        pp.print( "0x%x...0x%x [0x%x]"%(
            self._kstruct['start'], self._kstruct['end'], 
            self._kstruct['end'] - self._kstruct['start']))
        
        pp.printf("attributes: %s (0x%x)", ", ".join([self.get_vmr_kind(), self.get_protection()]), self.__attr)
        
        file = self._kstruct["mfile"]
        if file == 0:
            pp.print("anonymous region")
        else:
            pp.print("file mapped:")
            ppp = pp.next_level()
            ppp.print("dnode: %s @0x%x"%(get_dnode_path(file["dnode"]), file))
            ppp.print("range: 0x%x+0x%x"%(self._kstruct["foff"], self._kstruct["flen"]))
    
    def get_protection(self):
        attr_str = []
        if (self.__attr & (1 << 2)):
            attr_str.append("R")
        if (self.__attr & (1 << 3)):
            attr_str.append("W")
        if (self.__attr & (1 << 4)):
            attr_str.append("X")
        return ''.join(attr_str)
    
    def get_vmr_kind(self):
        """
            #define REGION_TYPE_CODE (1 << 16)
            #define REGION_TYPE_GENERAL (2 << 16)
            #define REGION_TYPE_HEAP (3 << 16)
            #define REGION_TYPE_STACK (4 << 16)
        """
        types = ["exec", "data", "heap", "stack"]
        attr = ((self.__attr >> 16) & 0xf) - 1
        if attr >= len(types):
            return "unknown kind %d"%attr
        return types[attr]
    

    
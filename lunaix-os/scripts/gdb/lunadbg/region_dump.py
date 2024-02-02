import gdb
from .utils import pid_argument, llist_foreach

def parse_type(attr):
    """
        #define REGION_TYPE_CODE (1 << 16)
        #define REGION_TYPE_GENERAL (2 << 16)
        #define REGION_TYPE_HEAP (3 << 16)
        #define REGION_TYPE_STACK (4 << 16)
        #define REGION_TYPE_VARS (5 << 16)
    """
    attr = (attr >> 16) & 0xf
    return ["exec", ]

class RegionDumpHelper:
    def __region_callback(self, idx, region, ident):
        print(f"VMR #{idx}:")
        print( "  0x%x...0x%x [0x%x]"%(
            region['start'], region['end'], 
            region['end'] - region['start']))
        
        attr = region["attr"]
        attr_str = []
        if (attr & (1 << 2)):
            attr_str.append("R")
        if (attr & (1 << 3)):
            attr_str.append("W")
        if (attr & (1 << 4)):
            attr_str.append("X")
        print( "  attr: 0x%x (%s)"%(attr, "".join(attr_str)))
        
        file = region["mfile"]
        if file == 0:
            print( "  anonymous region")
        else:
            print( "  file mapped:")
            print( "     dnode: %s @0x%x"%(file["dnode"]["name"]["value"].string(), file))
            print( "     range: 0x%x+0x%x"%(region["foff"], region["flen"]))

class MemoryRegionDump(gdb.Command):
    """Dump virtual memory regions associated with a process"""
    def __init__(self) -> None:
        super().__init__("vmrs", gdb.COMMAND_USER)

    

    

    def invoke(self, argument: str, from_tty: bool) -> None:
        argument = pid_argument(argument)
        
        pid = gdb.parse_and_eval(f"{argument}->pid")

        argument = f"&{argument}->mm.regions"
        val = gdb.parse_and_eval(argument)
        region_t = gdb.lookup_type("struct mm_region").pointer()
        
        print("VMRS (pid: %d)"%(pid))

        llist_foreach(val, region_t, "head", lambda a,b: self.region_callback(a,b))

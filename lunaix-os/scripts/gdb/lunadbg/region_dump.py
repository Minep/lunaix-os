import gdb
from .utils import pid_argument, llist_foreach
from .structs.region import MemRegion
from .pp import MyPrettyPrinter

class MemoryRegionDump(gdb.Command):
    """Dump virtual memory regions associated with a process"""
    def __init__(self) -> None:
        super().__init__("vmrs", gdb.COMMAND_USER)

    def region_callback(self, pp, idx, region):
        pp.print(f"VMR #{idx}:")
        ppp = pp.next_level()
        vmr = MemRegion(region)
        vmr.print_detailed(ppp)

    def invoke(self, argument: str, from_tty: bool) -> None:
        argument = pid_argument(argument)
        
        pid = gdb.parse_and_eval(f"{argument}->pid")

        argument = f"&{argument}->mm->regions"
        val = gdb.parse_and_eval(argument)
        region_t = gdb.lookup_type("struct mm_region").pointer()
        
        pp = MyPrettyPrinter()
        pp.print("VMRS (pid: %d)"%(pid))
        
        num = llist_foreach(val, region_t, "head", lambda a,b: self.region_callback(pp, a,b), inclusive=False)
        if not num:
            pp.print("no regions")

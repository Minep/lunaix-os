from .commands import LunadbgCommand
from .structs.pagetable import PageTable, PageTableEntry
from .pp import MyPrettyPrinter
from gdb import parse_and_eval, lookup_type

class PteInterpreter(LunadbgCommand):
    def __init__(self) -> None:
        super().__init__("pte")

        self._parser.description = "Interpret the PTE based on give raw value or ptep"
        self._parser.add_argument("val")
        self._parser.add_argument("--va", action='store_true', default=False,
                                    help="treat the given as virtual address")
        
        self._parser.add_argument("--ptep", action='store_true', default=False,
                                    help="treat the given as ptep")
        
        self._parser.add_argument('-l', "--at-level", type=int, default=-1,
                                    help="translation level that the given virtual address located")

        self._parser.add_argument('-m', "--mnt", default=-1,
                                    help="vms mount point that the given virtual address located")
    
    @staticmethod
    def print_pte(pp, pte_val, level):
        pte = PageTableEntry.from_pteval(pte_val, level)
        pp.print(pte)

    @staticmethod
    def print_ptep(pp, ptep, level):
        pte = PageTableEntry(ptep, level)
        pp.print(pte)

    def on_execute(self, parsed, gdb_args, from_tty):
        pp     = MyPrettyPrinter()

        val  = int(parse_and_eval(parsed.val))
        lvl =  parsed.at_level
        if not parsed.va:
            PteInterpreter.print_pte(pp, val, lvl)
            return
        
        if not parsed.ptep:
            ptep = PageTable.mkptep_at(parsed.mnt, val, lvl)
            PteInterpreter.print_ptep(pp, ptep, lvl)
            return
        
        PteInterpreter.print_ptep(pp, val, lvl)

class PtepInterpreter(LunadbgCommand):
    def __init__(self) -> None:
        super().__init__("ptep")
        
        self._parser.description = "Manipulate the pte pointer"
        self._parser.add_argument("ptep")
        self._parser.add_argument("--pfn", action='store_true', default=False,
                                    help="get the pfn (relative to mount point) implied by this ptep")
        
        self._parser.add_argument("--vfn", action='store_true', default=False,
                                    help="get the vfn implied by this ptep")
        
        self._parser.add_argument("--level", action='store_true', default=False,
                                    help="estimate the translation level implied by this ptep")
        
        self._parser.add_argument("--to-level", type=int, default=None,
                                    help="convert given ptep to specified level before any other processing")
        
        self._parser.add_argument("--sn", action='store_true', default=False,
                                    help="shift the ptep to next translation level")
        
        self._parser.add_argument("--sp", action='store_true', default=False,
                                    help="shift the ptep to previous translation level")
        

    def on_execute(self, parsed, gdb_args, from_tty):
        pp     = MyPrettyPrinter()

        ptep = int(parse_and_eval(parsed.ptep))
        if parsed.to_level is not None:
            ptep = PageTable.get_lntep(ptep, parsed.to_level)
            pp.printf("ptep: 0x%016x", ptep)

        if parsed.pfn:
            ptep = PageTable.get_pfn(ptep)
            pp.set_prefix("pfn: ")
        elif parsed.vfn:
            ptep = PageTable.get_vfn(ptep)
            pp.set_prefix("vfn: ")
        elif parsed.level:
            l, m = PageTable.ptep_infer_level(ptep)
            pp.printf("Level %d ptep (mnt=0x%016x, vfn=%d)", l, m, PageTable.get_vfn(ptep))
            return
        elif parsed.sn:
            ptep = PageTable.shift_ptep_nextlevel(ptep)
        elif parsed.sp:
            ptep = PageTable.shift_ptep_prevlevel(ptep)
        
        pp.printf("0x%016x", ptep)

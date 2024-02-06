from .commands import LunadbgCommand
from .pp import MyPrettyPrinter
from .profiling.pmstat import PhysicalMemProfile
from .structs.pagetable import PageTable, PageTableEntry

class MMStats(LunadbgCommand):
    def __init__(self) -> None:
        super().__init__("mm")
        subparsers = self._parser.add_subparsers(dest="cmd")
        
        stats = subparsers.add_parser("stats")
        stats.add_argument("state_type")
        stats.add_argument("-g", "--granule", type=int, default=512)
        stats.add_argument("--cols", type=int, default=32)

        lookup = subparsers.add_parser("lookup")
        lookup.add_argument("mem_type")
        lookup.add_argument("address")
        lookup.add_argument("-l", "--level", type=int, default=-1)
        lookup.add_argument("-n", type=int, default=0)
        lookup.add_argument("-t", "--to", dest="to_addr", default='0')

        self.__ptw = PageTable()

    def print_pmem_stats(self, pp: MyPrettyPrinter, optn):
        pmem = PhysicalMemProfile()
        pmem.rescan_pmem(optn.granule)

        pp.printf("Total: %dKiB (%d@4K)", 
                  pmem.max_mem_sz, pmem.max_mem_pg)
        
        pp.printf("Used:  %dKiB (%d@4K) ~%.2f%%", 
                  pmem.consumed_pg * 4096, 
                  pmem.consumed_pg, pmem.utilisation * 100)
        
        pp.printf("Fragmentations: %d ~%.2f%%", pmem.discontig, pmem.fragmentation * 100)
        pp.print()
        
        pp.print("Distribution")
        pp2 = pp.next_level(2)
        row = []
        for i in range(0, len(pmem.mem_distr)):
            ratio = pmem.mem_distr[i] / pmem.page_per_granule
            cat = int(ratio * 9)
            if ratio == 0:
                row.append('.')
            elif ratio == 1:
                row.append('F')
            else:
                row.append(str(cat))
                
            if (i + 1) % optn.cols == 0:
                pp2.print(''.join(row))
                row.clear()
        if (i + 1) % optn.cols != 0:
            pp2.print(''.join(row))
        
        pp.printf("(granule: %d, density: %d@4K)", optn.granule, pmem.page_per_granule)

    def vm_lookup(self, pp, va, optn):
        to_addr = int(optn.to_addr, 0)
        if not optn.n and not to_addr:
            pp.print(self.__ptw.get_pte(va, level=optn.level))
        else:
            if to_addr:
                self.__ptw.print_ptes_between(pp, va, to_addr, optn.level)
            else:
                self.__ptw.print_ptes(pp, va, optn.n, optn.level)

    def __do_stats(self, pp, optn):
        if optn.state_type == "pmem":
            self.print_pmem_stats(pp, optn)
        else:
            print("unknow stats type:", optn.state_type)

    def __do_lookup(self, pp, address, optn):
        if optn.mem_type == "vm":
            self.vm_lookup(pp, int(address, base=0), optn)
        else:
            print("unknow mem type:", optn.state_type)

    def invoke(self, argument: str, from_tty: bool) -> None:
        optn = self._parse_args(argument)
        pp = MyPrettyPrinter()

        if optn.cmd == 'stats':
            self.__do_stats(pp, optn)
        elif optn.cmd == 'lookup':
            self.__do_lookup(pp, optn.address, optn)
        else:
            print("unknown command:", optn.cmd)
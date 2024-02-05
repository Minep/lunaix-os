from gdb import Type, Value, lookup_type
from . import KernelStruct

MAX_LEVEL = 2

class PageTableEntry(KernelStruct):
    def __init__(self, gdb_inferior: Value, level, va) -> None:
        if level == -1:
            self.level = MAX_LEVEL
        else:
            self.level = level

        self.pg_mask = self.get_page_mask(level)
        self.va = va & ~self.pg_mask

        ptep = gdb_inferior[va // (self.pg_mask + 1)].address
        super().__init__(ptep, PageTableEntry)
        
        try:
            self.pte = int(self._kstruct.dereference())
        except:
            self.pte = 0

    def print_abstract(self, pp, *args):
        self.print_detailed(pp, *args)

    def print_simple(self, pp, *args):
        self.print_detailed(pp, *args)

    def print_detailed(self, pp, *args):
        if not self.pte:
            pp.print("<Mapping not exists>")
            return
        
        pa = self.pte & ~0xfff
        size = 0x1000
        if self.check_huge_page(self.pte):
            size = self.pg_mask + 1
        
        pp.printf("Level %d Translation", self.level)

        pp2 = pp.next_level()
        pp2.printf("Table entry: 0x%x", self.pte)
        pp2.printf("Virtual address: 0x%x (self.ptep=0x%x)", self.va, int(self._kstruct))
        pp2.printf("Mapped physical: 0x%x (order=0x%x)", pa, size)
        pp2.printf("Protection: %s", self.get_page_prot(self.pte))
        pp2.printf("Present: %s", self.check_presence(self.pte))
        pp2.printf("Huge: %s", self.check_huge_page(self.pte))
        pp2.print("All attributes:")
        pp2.next_level().print(self.get_attributes(self.pte))

    @staticmethod
    def get_type() -> Type:
        return lookup_type("unsigned int").pointer()
    
    def check_huge_page(self, pte):
        return bool(pte & (1 << 7))

    def get_page_mask(self, level):
        return PageTableEntry.get_level_shift(level) - 1
    
    def check_presence(self, pte):
        return bool(pte & 1)
    
    def get_page_prot(self, pte):
        prot = ['R'] # RWXUP
        if (pte & (1 << 1)):
            prot.append('W')
        if (pte & -1):
            prot.append('X')
        if (pte & (1 << 2)):
            prot.append('U')
        if (pte & (1)):
            prot.append('P')
        return ''.join(prot)

    def get_attributes(self, pte):
        attrs = [self.get_page_prot(pte)]
        if (pte & (1 << 5)):
            attrs.append("accessed")
        if (pte & (1 << 6)):
            attrs.append("dirty")
        if (pte & (1 << 3)):
            attrs.append("write_through")
        if (pte & (1 << 4)):
            attrs.append("no_cache")
        if (self.level == MAX_LEVEL and pte & (1 << 8)):
            attrs.append("global")
        return ', '.join(attrs)
    
    def non_null(self):
        return bool(self.pte)
    
    def same_kind_to(self, pte2):
        attr_mask = 0x19f  # P, R/W, U/S, PWT, PCD, PS, G
        return (self.pte & attr_mask) == (pte2.pte & attr_mask)
    
    @staticmethod
    def get_level_shift(level):
        if level == -1:
            return 0x1000
        else:
            return (1 << (12 + 9 * (2 - level - 1)))

class PageTable():
    def __init__(self) -> None:
        self.levels = [
            Value(0xFFFFF000).cast(PageTableEntry.get_type()),
            Value(0xFFC00000).cast(PageTableEntry.get_type())
        ]

    def get_pte(self, va, level=-1) -> PageTableEntry:
        return PageTableEntry(self.levels[level], level, va)
    
    def __print_pte_ranged(self, pp, pte_head, pte_tail):
        start_va = pte_head.va
        end_va = pte_tail.va
        sz = end_va - start_va
        if pte_head.non_null() and pte_tail.non_null():
            pp.printf("0x%016x...0x%016x [0x%08x] %s", 
                    start_va, end_va - 1, sz, 
                    pte_head.get_attributes(pte_head.pte))
        else:
            pp.printf("0x%016x...0x%016x [0x%08x] <no mapping>", 
                    start_va, end_va - 1, sz)
    
    def print_ptes_between(self, pp, va, va_end, level=-1):
        shift = PageTableEntry.get_level_shift(level)
        n = (va_end - va) // shift
        self.print_ptes(pp, va, n, level)

    def print_ptes(self, pp, va, pte_num, level=-1):
        head_pte = PageTableEntry(self.levels[level], level, va)
        curr_pte = head_pte
        va = head_pte.va
        for i in range(1, pte_num):
            va_ = va + i * PageTableEntry.get_level_shift(level)
            curr_pte = PageTableEntry(self.levels[level], level, va_)
            
            if not curr_pte.same_kind_to(head_pte):                
                self.__print_pte_ranged(pp, head_pte, curr_pte)
                head_pte = curr_pte
        
        if curr_pte != head_pte:
            self.__print_pte_ranged(pp, head_pte, curr_pte)
from gdb import Type, Value, lookup_type
from . import KernelStruct
from ..arch import PageTableHelper as TLB

class PageTableEntry(KernelStruct):
    def __init__(self, gdb_inferior: Value, level, va) -> None:
        self.level = level
        self.pg_mask = self.get_page_mask()
        self.va = va & ~self.pg_mask
        self.base_page_order = TLB.translation_shift_bits(-1)

        ptep = gdb_inferior[va // (self.pg_mask + 1)].address
        super().__init__(ptep, PageTableEntry)
        
        try:
            self.pte = int(self._kstruct.dereference())
        except:
            self.pte = 0

        self.pa = TLB.physical_pfn(self.pte) << self.base_page_order

    def print_abstract(self, pp, *args):
        self.print_detailed(pp, *args)

    def print_simple(self, pp, *args):
        self.print_detailed(pp, *args)

    def print_detailed(self, pp, *args):
        if self.null():
            pp.print("<Mapping not exists>")
            return
        
        page_order = TLB.translation_shift_bits(self.level)
        page_order -= self.base_page_order
        
        pp.printf("Level %d Translation", TLB.translation_level(self.level))

        pp2 = pp.next_level()
        pp2.printf("Entry value: 0x%x", self.pte)
        pp2.printf("Virtual address: 0x%x (ptep=0x%x)", self.va, int(self._kstruct))
        pp2.printf("Mapped physical: 0x%x (order %d page)", self.pa, page_order)
        pp2.printf("Page Protection: %s", self.get_page_prot())
        pp2.printf("Present: %s", self.present())
        pp2.printf("Huge: %s", TLB.huge_page(self.pte))
        pp2.print("Attributes:")
        pp2.next_level().print(self.get_attributes())

    @staticmethod
    def get_type() -> Type:
        return lookup_type("unsigned int").pointer()

    def get_page_mask(self):
        return PageTableEntry.get_level_shift(self.level) - 1
    
    def present(self):
        return TLB.mapping_present(self.pte)
    
    def get_page_prot(self):
        return ''.join(TLB.protections(self.pte))

    def get_attributes(self):
        attrs = [ self.get_page_prot(),
                  *TLB.other_attributes(self.level, self.pte) ]
        return ', '.join(attrs)
    
    def null(self):
        return TLB.null_mapping(self.pte)
    
    def same_kind_to(self, pte2):
        return TLB.same_kind(self.pte, pte2.pte)
    
    @staticmethod
    def get_level_shift(level):
        return 1 << TLB.translation_shift_bits(level)
    
    @staticmethod
    def max_page_count():
        return 1 << (TLB.vaddr_width() - TLB.translation_shift_bits(-1))

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
        if not (pte_head.null() and pte_tail.null()):
            pp.printf("0x%016x...0x%016x, 0x%016x [0x%08x] %s", 
                    start_va, end_va - 1, pte_head.pa, sz,
                    pte_head.get_attributes())
        else:
            pp.printfa("0x{:016x}...0x{:016x}, {:^18s} [0x{:08x}] <no mapping>", 
                    start_va, end_va - 1, "n/a", sz)
    
    def print_ptes_between(self, pp, va, va_end, level=-1):
        shift = PageTableEntry.get_level_shift(level)
        n = (va_end - va) // shift
        self.print_ptes(pp, va, n, level)

    def print_ptes(self, pp, va, pte_num, level=-1):
        head_pte = PageTableEntry(self.levels[level], level, va)
        curr_pte = head_pte
        va = head_pte.va

        pp.printfa("{:^18s}   {:^18s}  {:^18s}  {:^10s}  {:^20s}", 
                   "va-start", "va-end", "physical", "size", "attributes")
        for i in range(1, pte_num):
            va_ = va + i * PageTableEntry.get_level_shift(level)
            curr_pte = PageTableEntry(self.levels[level], level, va_)
            
            if not curr_pte.same_kind_to(head_pte):                
                self.__print_pte_ranged(pp, head_pte, curr_pte)
                head_pte = curr_pte
        
        if curr_pte != head_pte:
            self.__print_pte_ranged(pp, head_pte, curr_pte)
from gdb import Type, Value, lookup_type
from . import KernelStruct
from ..arch import PageTableHelper as TLB

class PageTableEntry(KernelStruct):
    def __init__(self, ptep, level, pte=None) -> None:
        self.level = level
        self.base_page_order = TLB.translation_shift_bits(-1)
        self.ptep = ptep

        super().__init__(Value(ptep), PageTableEntry)

        if pte:
            self.pte = pte
            self.va  = None
        else:
            self.va = PageTable.va_at(ptep, level)
            try:
                self.pte = int(self._kstruct['val'])
            except:
                self.pte = 0

        self.pa = TLB.physical_pfn(self.pte) << self.base_page_order

        self.page_order = TLB.translation_shift_bits(self.level)
        self.page_size = 1 << self.page_order
        self.page_order -= self.base_page_order

    @staticmethod
    def from_pteval(pte_val, level):
        return PageTableEntry(0, level, pte=pte_val)

    def print_abstract(self, pp, *args):
        self.print_detailed(pp, *args)

    def print_simple(self, pp, *args):
        self.print_detailed(pp, *args)

    def print_detailed(self, pp, *args):
        if self.null():
            pp.print("<Mapping not exists>")
            return
        
        pp.printf("Level %d Translation", TLB.translation_level(self.level))

        pp2 = pp.next_level()
        pp2.printf("PTE raw value: 0x%016x", self.pte)
        
        if not self.va:
            pp2.printf("Virtual address: <n/a> (ptep=<n/a>)")
        else:
            pp2.printf("Virtual address: 0x%016x (ptep=0x%016x)", self.va, int(self._kstruct))

        pp2.printf("Mapped physical: 0x%016x (order %d page)", self.pa, self.page_order)
        pp2.printf("Page Protection: %s", self.get_page_prot())
        pp2.printf("Present: %s", self.present())
        pp2.printf("Huge: %s", TLB.huge_page(self.pte, self.page_order))
        pp2.print("Attributes:")
        pp2.next_level().print(self.get_attributes())
    
    def leaf(self):
        return TLB.huge_page(self.pte, self.page_order) or not self.page_order

    @staticmethod
    def get_type() -> Type:
        return lookup_type("pte_t").pointer()

    def get_page_mask(self):
        return PageTableEntry.get_level_shift(self.level) - 1
    
    def present(self):
        return TLB.mapping_present(self.pte)
    
    def get_page_prot(self):
        return ''.join(TLB.protections(self.pte))

    def get_attributes(self):
        attrs = [ self.get_page_prot(),
                  *TLB.other_attributes(self.level, self.pte),
                  "leaf" if self.leaf() else "root" ]
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
    
    def pfn(self):
        return (self.ptep & (((1 << TLB.translation_shift_bits(0)) - 1))) // TLB.pte_size()
    
    def vfn(self):
        return (self.ptep & (((1 << TLB.translation_shift_bits(-1)) - 1))) // TLB.pte_size()

class PageTable():
    def __init__(self) -> None:
        pass

    @staticmethod
    def get_pfn(ptep):
        pfn_mask = ((1 << TLB.translation_shift_bits(0)) - 1)
        return ((ptep & pfn_mask) // TLB.pte_size())
    
    @staticmethod
    def get_vfn(ptep):
        vfn_mask = ((1 << TLB.translation_shift_bits(-1)) - 1)
        return ((ptep & vfn_mask) // TLB.pte_size())

    @staticmethod
    def mkptep_for(mnt, va):
        mnt_mask = ~((1 << TLB.translation_shift_bits(0)) - 1)
        offset   = (TLB.physical_pfn(va) * TLB.pte_size()) & ~mnt_mask

        return (mnt & mnt_mask) | offset

    @staticmethod
    def ptep_infer_level(ptep):
        l = 0
        pfn = PageTable.get_pfn(ptep)
        pfn = pfn << TLB.translation_shift_bits(-1)
        vfn = (TLB.pgtable_len() - 1)
        msk = vfn << TLB.translation_shift_bits(l)
        max_l = TLB.translation_level()

        mnt = ptep & msk

        while (pfn & msk) == msk:
            l+=1
            msk = vfn << TLB.translation_shift_bits(l)
            if l == max_l:
                break

        return (max_l - l, mnt)

    @staticmethod
    def va_at(ptep, level):
        vms_mask = ((1 << TLB.vaddr_width()) - 1)

        ptep = PageTable.get_pfn(ptep) << TLB.translation_shift_bits(level)
        return ptep & vms_mask
    
    @staticmethod
    def get_l0tep(ptep):
        return PageTable.get_lntep(ptep, 0)
    
    @staticmethod
    def get_lntep(ptep, level):
        lnmask = (1 << TLB.translation_shift_bits(level)) - 1
        size = (1 << TLB.translation_shift_bits(-1))
        vpfn = (lnmask * size) & lnmask
        offset = ((ptep // TLB.pte_size()) * size // (lnmask + 1)) & (size - 1)

        return (ptep & ~lnmask) | vpfn | (offset * TLB.pte_size())
    
    @staticmethod
    def mkptep_at(mnt, va, level):
        lfmask = (1 << TLB.translation_shift_bits(-1)) - 1
        lsize = (1 << TLB.translation_shift_bits(level))
        offset = (va // lsize) * TLB.pte_size()

        return mnt | ((lsize - 1) & ~lfmask) | offset
    
    @staticmethod
    def shift_ptep_nextlevel(ptep):
        mnt_mask = ~((1 << TLB.translation_shift_bits(0)) - 1)
        size     = (1 << TLB.translation_shift_bits(-1))
        mnt      = ptep & mnt_mask
        vpfn     = ((ptep // TLB.pte_size()) * size) & ~mnt_mask

        return mnt | vpfn

    @staticmethod
    def shift_ptep_prevlevel(ptep):
        mnt_mask = ~((1 << TLB.translation_shift_bits(0)) - 1)
        self_mnt = (TLB.pgtable_len() - 1) * (~mnt_mask + 1)
        unshifted = PageTable.get_pfn(ptep) << TLB.translation_shift_bits(-1)
        unshifted = PageTable.mkptep_for(self_mnt, unshifted)
        return PageTable.mkptep_for(ptep & mnt_mask, unshifted)
    
    def __print_pte_ranged(self, pp, pte_head, pte_tail):
        start_va = pte_head.va

        if pte_head == pte_tail:
            end_va = pte_head.va + pte_head.page_size
        else:
            end_va = pte_tail.va

        sz = end_va - start_va
        if not pte_head.null():
            pp.printf("0x%016x...0x%016x, 0x%016x [0x%08x] %s", 
                    start_va, end_va - 1, pte_head.pa, sz,
                    pte_head.get_attributes())
        else:
            pp.printfa("0x{:016x}...0x{:016x}, {:^18s} [0x{:08x}] <no mapping>", 
                    start_va, end_va - 1, "n/a", sz)
    
    def __scan_pagetable(self, pp, start_ptep, end_ptep, max_level = -1):
        ptep  = PageTable.get_l0tep(start_ptep)
        level = 0
        max_level = TLB.translation_level(max_level)
        va_end = PageTable.va_at(end_ptep, -1) + 1

        head_pte = None
        prev_pte = None
        pp.printfa("{:^18s}   {:^18s}  {:^18s}  {:^10s}  {:^20s}", 
                   "va-start", "va-end", "physical", "size", "attributes")
        while PageTable.va_at(ptep, level) <= va_end:
            pte = PageTableEntry(ptep, level)
            if head_pte == None:
                head_pte = pte
                prev_pte = pte

            if pte.null():
                if not head_pte.null():
                    self.__print_pte_ranged(pp, head_pte, prev_pte)
                    head_pte = pte
            elif not pte.leaf() and level < max_level:
                ptep = PageTable.shift_ptep_nextlevel(ptep)
                level+=1
                continue
            else:
                if head_pte.null():
                    self.__print_pte_ranged(pp, head_pte, prev_pte)
                    head_pte = pte
                else:
                    n = pte.pfn() - head_pte.pfn()
                    pa = head_pte.pa + (n << pte.base_page_order)
                    if pa != pte.pa or not pte.same_kind_to(head_pte):
                        self.__print_pte_ranged(pp, head_pte, prev_pte)
                        head_pte = pte

            prev_pte = pte
            if pte.vfn() == TLB.pgtable_len() - 1:
                if level != 0:
                    ptep = PageTable.shift_ptep_prevlevel(ptep + TLB.pte_size())
                    level-=1
                    continue
                break
            
            ptep += TLB.pte_size()

        
        self.__print_pte_ranged(pp, head_pte, prev_pte)
                    
    def print_ptes_between(self, pp, va, va_end, level=-1, mnt=0xFFC00000):
        ptep_start = PageTable.mkptep_for(mnt, va)
        ptep_end = PageTable.mkptep_for(mnt, va_end)
        self.__scan_pagetable(pp, ptep_start, ptep_end, level)

    def get_pte(self, va, level=-1, mnt=0xFFC00000) -> PageTableEntry:
        ptep = PageTable.mkptep_at(mnt, va, level)
        return PageTableEntry(ptep, level)
    
    def print_ptes(self, pp, va, pte_num, level=-1, mnt=0xFFC00000):
        ptep_start = PageTable.mkptep_for(mnt, va)
        ptep_end = ptep_start + pte_num * TLB.pte_size()
        self.__scan_pagetable(pp, ptep_start, ptep_end, level)
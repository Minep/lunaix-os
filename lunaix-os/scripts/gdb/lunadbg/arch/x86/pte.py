class PageTableHelperBase:
    @staticmethod
    def null_mapping(pte):
        return pte == 0
    
    @staticmethod
    def translation_level(level=-1):
        raise NotImplementedError()
    
    @staticmethod
    def translation_shift_bits(level):
        raise NotImplementedError()
    
    @staticmethod
    def mapping_present(pte):
        raise NotImplementedError()
    
    @staticmethod
    def huge_page(pte):
        raise NotImplementedError()
    
    @staticmethod
    def protections(pte):
        raise NotImplementedError()
    
    @staticmethod
    def other_attributes(level, pte):
        raise NotImplementedError()
    
    @staticmethod
    def same_kind(pte1, pte2):
        raise NotImplementedError()
    
    @staticmethod
    def physical_pfn(pte):
        raise NotImplementedError()
    
    @staticmethod
    def vaddr_width():
        raise NotImplementedError()
    
    @staticmethod
    def pte_size():
        raise NotImplementedError()

class PageTableHelper32(PageTableHelperBase):
    @staticmethod
    def translation_level(level = -1):
        return [0, 1][level]
    
    @staticmethod
    def pgtable_len():
        return (1 << 10)
    
    @staticmethod
    def translation_shift_bits(level):
        return [10, 0][level] + 12
    
    @staticmethod
    def mapping_present(pte):
        return bool(pte & 1)
    
    @staticmethod
    def huge_page(pte, po):
        return bool(pte & (1 << 7)) and po 
    
    @staticmethod
    def protections(pte):
        prot = ['R'] # RWXUP
        if (pte & (1 << 1)):
            prot.append('W')
        if (pte & -1):
            prot.append('X')
        if (pte & (1 << 2)):
            prot.append('U')
        if (pte & (1)):
            prot.append('P')
        return prot
    
    @staticmethod
    def other_attributes(level, pte):
        attrs = []
        if pte & (1 << 5):
            attrs.append("A")
        if pte & (1 << 6):
            attrs.append("D")
        if pte & (1 << 3):
            attrs.append("PWT")
        if pte & (1 << 4):
            attrs.append("PCD")
        if PageTableHelper32.translation_level(level) == 1 and pte & (1 << 8):
            attrs.append("G")
        return attrs
    
    @staticmethod
    def same_kind(pte1, pte2):
        attr_mask = 0x19f  # P, R/W, U/S, PWT, PCD, PS, G
        return (pte1 & attr_mask) == (pte2 & attr_mask)
    
    @staticmethod
    def physical_pfn(pte):
        return pte >> 12
    
    @staticmethod
    def vaddr_width():
        return 32
    
    @staticmethod
    def pte_size():
        return 4

class PageTableHelper64(PageTableHelperBase):
    pass
from ..symbols import LunaixSymbols
from ..structs.page import PageStruct
from ..structs.pmem import PMem
from ..pp import MyPrettyPrinter
import math

ENTER_CONTIG = 0
LEAVE_CONTIG = 1

class PhysicalMemProfile:
    def __init__(self) -> None:
        super().__init__()
        self._pmem    = PMem(LunaixSymbols.debug_sym("pmm", "memory").value().address)

        self.max_mem_pg = self._pmem.list_len()
        self.max_mem_sz = self.max_mem_pg * 4096
        self.mem_distr = []

    def rescan_pmem(self, distr_granule = 256):
        self.__mem_distr_granule = distr_granule
        self.mem_distr.clear()

        pplist = self._pmem.pplist()
        page_per_granule = int(self.max_mem_pg) // self.__mem_distr_granule
        remainder = self.max_mem_pg % self.__mem_distr_granule
        bucket = 0
        non_contig = 0
        contig_state = LEAVE_CONTIG
        new_state = LEAVE_CONTIG

        i = 0
        while i < self.max_mem_pg:
            element = PageStruct(pplist[i].address)

            nr_pgs = 1
            if element.lead_page():
                nr_pgs = 1 << element.order
                if element.busy():
                    bucket += nr_pgs
                    new_state = ENTER_CONTIG
                else:
                    new_state = LEAVE_CONTIG
            
            i += nr_pgs

            if contig_state != new_state:
                non_contig += int(new_state == LEAVE_CONTIG)
                contig_state = new_state

            if i % page_per_granule == 0:
                self.mem_distr.append(bucket)
                bucket = 0
        
        if remainder > 0:
            if bucket > 0:
                bucket += page_per_granule - remainder
            self.mem_distr.append(bucket)

        self.consumed_pg = sum(self.mem_distr)
        self.utilisation = self.consumed_pg / self.max_mem_pg
        self.fragmentation = 2 * non_contig / self.max_mem_pg
        self.discontig = non_contig
        self.page_per_granule = page_per_granule
    
from .region_dump import MemoryRegionDump
from .proc_table_dump import ProcessDump, ProcessTableDump


def load_commands():
    MemoryRegionDump()
    ProcessTableDump()
    ProcessDump()
from .region_dump import MemoryRegionDump
from .proc_table_dump import ProcessDump, ProcessTableDump
from .syslog import SysLogDump


def load_commands():
    MemoryRegionDump()
    ProcessTableDump()
    ProcessDump()
    SysLogDump()
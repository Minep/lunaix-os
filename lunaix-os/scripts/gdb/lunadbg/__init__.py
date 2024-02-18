import os

if "LUNADBG_ARCH" not in os.environ:
    os.environ["LUNADBG_ARCH"] = "x86_32"

from .region_dump import MemoryRegionDump
from .sched_dump import ProcessDump, SchedulerDump
from .mem import MMStats
from .syslog import SysLogDump
from .pte_utils import PteInterpreter, PtepInterpreter

MemoryRegionDump()
SchedulerDump()
ProcessDump()
SysLogDump()
MMStats()
PtepInterpreter()
PteInterpreter()
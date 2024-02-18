import gdb
from .utils import pid_argument, llist_foreach
from .pp import MyPrettyPrinter
from .structs.pcb import ProcInfo
from .structs.scheduler import Scheduler
from .commands import LunadbgCommand
       
class ProcessDump(LunadbgCommand):
    """Dump the state of Lunaix PCB"""
    def __init__(self) -> None:
        super().__init__("proc")

    def execute(self, parsed, gdb_args, from_tty):
        pp = MyPrettyPrinter()
        ProcInfo.process_at(gdb_args).print_detailed(pp)


class SchedulerDump(LunadbgCommand):
    """Dump the state of Lunaix process table"""
    def __init__(self) -> None:
        super().__init__("sched")
        self._parser.add_argument("print_type")
        self._parser.add_argument("-l", "--long-list", 
                                  required=False, default=False, action='store_true')

    def on_execute(self, args, gdb_args, from_tty):
        sched_context = gdb.parse_and_eval("&sched_ctx")
        sched = Scheduler(sched_context)

        sched.print_detailed(MyPrettyPrinter(), args.print_type, args.long_list)
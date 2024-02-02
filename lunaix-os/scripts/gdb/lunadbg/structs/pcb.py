import gdb
from ..pp import MyPrettyPrinter
from . import KernelStruct
from ..utils import pid_argument, llist_foreach, get_dnode_path


class ProcInfo(KernelStruct):
    def __init__(self, gdb_inferior: gdb.Value) -> None:
        super().__init__(gdb_inferior, ProcInfo)
    
    @staticmethod
    def get_type() -> gdb.Type:
        return gdb.lookup_type("struct proc_info").pointer()
    
    def print_detailed(self, pp : MyPrettyPrinter, *args):
        self.print_abstract(pp)
        
        pp2 = (pp.next_level()
                 .clear_prefix())
        self.print_simple(pp2)

    def print_simple(self, pp : MyPrettyPrinter, *args):
        pp.print_field(self._kstruct, 'pid')
        pp.print_field(self._kstruct, 'pgid')
        pp.print_field(self._kstruct, 'parent', fmt="(pid=%d)", cast=lambda v: v["pid"])
        pp.print_field(self._kstruct, 'cwd', cast=lambda v: get_dnode_path(v))
        pp.print_field(self._kstruct, 'created', '+%dms')
        pp.print_field(self._kstruct, 'exit_code')
        pp.print_field(self._kstruct, 'thread_count')
        
        pp.printf("active thread: (tid=%d)", self._kstruct['th_active']['tid'])

    def print_abstract(self, pp : MyPrettyPrinter, *args):
        pid = self._kstruct['pid']
        ppid = self._kstruct['parent']['pid']
        cmd = self._kstruct['cmd']
        cmd = cmd.string() if cmd != 0 else ''
        state = ProcInfo.get_state(self._kstruct["state"])
        
        pp.print(f"pid={pid}, ppid={ppid}, cmd='{cmd}', {state}")


    PS_READY   = 0
    PS_RUNNING = 1
    PS_TERMNAT = 2
    PS_DESTROY = 4
    PS_PAUSED  = 8
    PS_BLOCKED = 16
    PS_CREATED = 32

    @staticmethod
    def get_state(state_t):
        if (state_t == ProcInfo.PS_READY):
            return "ready"
        if (state_t == ProcInfo.PS_RUNNING):
            return "running"
        if (state_t & (ProcInfo.PS_TERMNAT | ProcInfo.PS_DESTROY)):
            return "terminated"
        if (state_t & ProcInfo.PS_BLOCKED):
            return "blocked"
        if (state_t & ProcInfo.PS_PAUSED):
            return "paused"
        return "<unknown> (0x%x)"%(state_t)
    
    @staticmethod
    def process_at(pid):
        return ProcInfo(gdb.parse_and_eval(pid_argument(pid)))
    
    
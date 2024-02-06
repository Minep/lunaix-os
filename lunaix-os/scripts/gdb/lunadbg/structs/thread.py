import gdb
from . import KernelStruct
from .pcb import ProcInfo
from .signal import SignalContext

class ThreadStruct(KernelStruct):
    def __init__(self, gdb_inferior: gdb.Value) -> None:
        super().__init__(gdb_inferior, ThreadStruct)

        self.__sigctx = SignalContext(self._kstruct["sigctx"].address)

    @staticmethod
    def get_type() -> gdb.Type:
        return gdb.lookup_type("struct thread").pointer()
    
    def print_abstract(self, pp, *args):
        tid = self._kstruct['tid']
        pid = self._kstruct['process']['pid']
        thactive = self._kstruct['process']['th_active']
        state = ProcInfo.get_state(self._kstruct['state'])

        notes = f"(acting, {state})" if thactive == self._kstruct else f"({state})"
        pp.print(f"Thread: tid={int(tid)}, pid={int(pid)}", notes)

        self.__sigctx.print_abstract(pp.next_level())
    
    def print_detailed(self, pp, *args):
        self.print_abstract(pp)

        pp2 = pp.next_level()
        self.print_simple(pp2)

        pp3 = pp2.next_level()
        pp2.print("Containing Process")
        pp3.print(ProcInfo(self._kstruct['process']))

        self.__sigctx.print_detailed(pp2)

    
    def print_simple(self, pp, *args):
        pp2 = pp.next_level()
        pp2.print_field(self._kstruct, 'created', '+%dms')
        pp2.print_field(self._kstruct, 'syscall_ret')
        pp2.print_field(self._kstruct, 'exit_val')
        pp2.print_field(self._kstruct, 'kstack')
        self.__sigctx.print_simple(pp2)

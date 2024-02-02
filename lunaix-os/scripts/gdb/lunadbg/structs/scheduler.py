import gdb
from . import KernelStruct
from .pcb import ProcInfo
from .thread import ThreadStruct
from ..utils import llist_foreach

class Scheduler(KernelStruct):
    def __init__(self, gdb_inferior:  gdb.Value) -> None:
        super().__init__(gdb_inferior, Scheduler)

        self._current_t = gdb.parse_and_eval("current_thread")
        self._current_p = gdb.parse_and_eval("__current")

    @staticmethod
    def get_type() -> gdb.Type:
        return gdb.lookup_type("struct scheduler").pointer()
    
    def __print_thread_cb(self, v, pp, long_list):
        pi = ThreadStruct(v)
        pi.print_abstract(pp)
        if long_list:
            pi.print_simple(pp.next_level())
        pp.print()


    def __print_threads(self, pp, long_list):
        pp.print("# of threads:", self._kstruct["ttable_len"])
        pp.printf("scheduled: pid=%d, tid=%d", self._current_p['pid'], self._current_t['tid'] )
        pp.print()

        pp2 = pp.next_level()
        plist = self._kstruct["threads"]
        llist_foreach(plist, ThreadStruct.get_type(), "sched_sibs", 
                      lambda i,v: self.__print_thread_cb(v, pp2, long_list))
        pp.print()
    

    def __print_proc_cb(self, v, pp, long_list):
        pi = ProcInfo(v)
        pi.print_abstract(pp)
        if long_list:
            pi.print_simple(pp.next_level())


    def __print_processes(self, pp, long_list = False):
        pp.print("# of process:", self._kstruct["ptable_len"])
        pp.printf("scheduled: pid=%d", self._current_p['pid'])
        pp.print()

        pp2 = pp.next_level()
        plist = self._kstruct["proc_list"]
        llist_foreach(plist, ProcInfo.get_type(), "tasks", 
                      lambda i,v: self.__print_proc_cb(v, pp2, long_list))
    

    def print_detailed(self, pp, *args):
        print_type = args[0]
        print_longlist = args[1]
        if print_type == 'procs':
            self.__print_processes(pp, print_longlist)
        elif print_type == 'threads':
            self.__print_threads(pp, print_longlist)
        else:
            pp.print("Unknown print type:", print_type)


    def print_abstract(self, pp, *args):
        self.print_detailed(pp, *args)
    

    def print_simple(self, pp, *args):
        self.print_detailed(pp, *args)
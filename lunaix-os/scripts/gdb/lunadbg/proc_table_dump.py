import gdb
from .utils import pid_argument, llist_foreach

class ProcessHelper:
    PS_READY   = 0
    PS_RUNNING = 1
    PS_TERMNAT = 2
    PS_DESTROY = 4
    PS_PAUSED  = 8
    PS_BLOCKED = 16
    PS_CREATED = 32

    def get_state(proc: gdb.Value):
        state_t = proc["state"]
        if (state_t == ProcessHelper.PS_READY):
            return "ready"
        if (state_t == ProcessHelper.PS_RUNNING):
            return "running"
        if (state_t & (ProcessHelper.PS_TERMNAT | ProcessHelper.PS_DESTROY)):
            return "terminated"
        if (state_t & ProcessHelper.PS_BLOCKED):
            return "blocked"
        if (state_t & ProcessHelper.PS_PAUSED):
            return "paused"
        
    def process_at(pid):
        return gdb.parse_and_eval(pid_argument(pid))
    
    def get_sig_bitmap(sigbmp):
        if sigbmp == 0:
            return '<None>'
        v = []
        i = 0
        while sigbmp != 0:
            if sigbmp & 1 != 0:
                v.append(str(i))
            sigbmp = sigbmp >> 1
            i+=1
        return ",".join(v)

    @staticmethod    
    def pp_process(proc: gdb.Value):
        print("  pid:", proc["pid"])
        print("  pgid:", proc["pgid"])
        if proc["parent"] == 0:
            print(" root process")
        else:
            print("  ppid:", proc["parent"]["pid"])

        print("  state:", ProcessHelper.get_state(proc))
        print("  created: +%dms"%(proc["created"]))

        print("   Threads:")

        thread = gdb.lookup_type("struct thread").pointer()
        active_th = proc["th_active"]
        llist_foreach(proc["threads"].address, thread, 'proc_sibs', lambda a,b: ProcessHelper.print_thread_cb(a,b, active_th))

    def print_thread_cb(index, val: gdb.Value, active: gdb.Value):
        addr1 = int(val.cast(gdb.lookup_type("void").pointer()))
        addr2 = int(active.cast(gdb.lookup_type("void").pointer()))
        isActive = addr1 == addr2
        print("      [Thread #%d] %s"%(index, "(active)" if isActive else ""))
        ProcessHelper.pp_thread(val, 3)

    @staticmethod
    def pp_thread(thread: gdb.Value, indent):
        pref = "   " * indent
        print(pref, "tid:", thread["tid"])
        print(pref, "state:", ProcessHelper.get_state(thread))
        print(pref, "created: +%dms"%(thread["created"]))
        print(pref, "kstack_top:", thread["kstack"])
        print(pref, "ustack_top:", thread["ustack"])
        print(pref, "sig_masked:", ProcessHelper.get_sig_bitmap(thread["sigctx"]["sig_mask"]))
        print(pref, "sig_pending:", ProcessHelper.get_sig_bitmap(thread["sigctx"]["sig_pending"]))
        


class ProcessDump(gdb.Command):
    """Dump the state of Lunaix PCB"""
    def __init__(self) -> None:
        super().__init__("proc", gdb.COMMAND_USER)

    def invoke(self, argument: str, from_tty: bool) -> None:
        argument = pid_argument(argument)

        proc = gdb.parse_and_eval(argument)

        ProcessHelper.pp_process(proc)

class ProcessTableDump(gdb.Command):
    """Dump the state of Lunaix process table"""
    def __init__(self) -> None:
        super().__init__("proc_table", gdb.COMMAND_USER)

    def invoke(self, argument: str, from_tty: bool) -> None:
        sched_context = gdb.parse_and_eval("sched_ctx")
        total_entries = sched_context["ptable_len"]
        print("inited entries: %d"%(total_entries))
        print("scheduled pid: %d"%(sched_context["procs_index"]))
        print("Process Table:")

        for i in range(0, total_entries):
            p = ProcessHelper.process_at(i)
            if (p == 0):
                continue
            state = ProcessHelper.get_state(p)
            print("   pid:%02d [%s]"%(i, state))
import gdb
from .symbols import LunaixSymbols, SymbolDomain
from .utils import llist_foreach

class SysLogDump(gdb.Command):
    """Dump the system log"""
    def __init__(self) -> None:
        super().__init__("syslog", gdb.COMMAND_USER)
        self.log_level = ["debug", "info", "warn", "error", "fatal"]

    def syslog_entry_callback(self, idx, ent):
        time = ent["time"]
        lvl = ent["lvl"]
        log = ent["content"]

        time_str = "%04d.%03d"%(int(time / 1000), time % 1000)
        print(f"[{time_str}] <L{self.log_level[lvl]}> {log.string()}")

    def invoke(self, argument: str, from_tty: bool) -> None:
        log_recs = LunaixSymbols.exported(SymbolDomain.DEBUG, "kprecs")
        head = log_recs.deref_and_access("kp_ents.ents").address

        ent_type = gdb.lookup_type("struct kp_entry").pointer()
        llist_foreach(head, ent_type, "ents", lambda a,b: self.syslog_entry_callback(a, b))
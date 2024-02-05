from gdb import Type, Value, lookup_type
from . import KernelStruct
from ..pp import MyPrettyPrinter

class SignalContext(KernelStruct):
    __SIGNUM = 16
    def __init__(self, gdb_inferior: Value) -> None:
        super().__init__(gdb_inferior, SignalContext)

    @staticmethod
    def get_type() -> Type:
        return lookup_type("struct sigctx").pointer()

    def print_abstract(self, pp : MyPrettyPrinter, *args):
        sigactive = self._kstruct["sig_active"]
        sigpending = SignalHelper.get_sig_bitmap(self._kstruct["sig_pending"])

        pp.print(f"sig: handling={sigactive}, pending=[{sigpending}]")
    
    def print_simple(self, pp : MyPrettyPrinter, *args):
        pp.print_field(self._kstruct, "sig_active")
        pp.print_field(self._kstruct, "sig_pending", cast=SignalHelper.get_sig_bitmap)
        pp.print_field(self._kstruct, "sig_mask", cast=SignalHelper.get_sig_bitmap)

        order = []
        active = int(self._kstruct['sig_active'])
        sig_order = self._kstruct["sig_order"]
        while active != 0:
            order.append(str(active))
            active = int(sig_order[active])

        pp.print("nestings:", ' -> '.join(reversed(order)))
    
    def print_detailed(self, pp, *args):
        self.print_simple(pp)
    


class SignalHelper:

    @staticmethod
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
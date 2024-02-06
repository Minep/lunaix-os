import gdb
from ..pp import PrettyPrintable


class KernelStruct(PrettyPrintable):
    def __init__(self, gdb_inferior: gdb.Value, impl) -> None:
        super().__init__()
        self._kstruct = gdb_inferior.cast(impl.get_type())

    def get_struct_instance(self):
        return self._kstruct

    @staticmethod
    def get_type() -> gdb.Type :
        return gdb.lookup_type("void").pointer()
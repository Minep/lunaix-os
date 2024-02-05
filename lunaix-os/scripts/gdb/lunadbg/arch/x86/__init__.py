import os

if os.environ["LUNADBG_ARCH"] == 'x86_32':
    from .pte import PageTableHelper32 as PageTableHelper
else:
    from .pte import PageTableHelper64 as PageTableHelper

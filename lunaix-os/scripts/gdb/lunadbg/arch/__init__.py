import os

if os.environ["LUNADBG_ARCH"].startswith("x86_"):
    from .x86 import *
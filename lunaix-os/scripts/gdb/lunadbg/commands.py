from gdb import Command, COMMAND_USER

import argparse

class LunadbgCommand(Command):
    def __init__(self, name: str) -> None:
        super().__init__(name, COMMAND_USER)
        self._parser = argparse.ArgumentParser()

    def _parse_args(self, gdb_argstr):
        args, argv = self._parser.parse_known_args(gdb_argstr.split(' '), None)
        if argv:
            print('unrecognized arguments: %s'%(' '.join(argv)))
            print(self._parser.format_usage())
            print(self._parser.format_help())
            return None
        return args
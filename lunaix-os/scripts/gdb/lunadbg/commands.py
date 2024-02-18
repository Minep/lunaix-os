from gdb import Command, COMMAND_USER

import argparse
import shlex

class LunadbgCommand(Command):
    def __init__(self, name: str) -> None:
        super().__init__(name, COMMAND_USER)
        self._parser = argparse.ArgumentParser()

    def _parse_args(self, gdb_argstr: str):
        try:
            args, argv = self._parser.parse_known_args(shlex.split(gdb_argstr), None)
            if argv:
                print('unrecognized arguments: %s'%(' '.join(argv)))
            else:
                return args
        except SystemExit:
            pass 
        return None
    
    def invoke(self, argument: str, from_tty: bool) -> None:
        parsed = self._parse_args(argument)
        if not parsed:
            return
        self.on_execute(parsed, argument, from_tty)
    
    def on_execute(self, parsed, gdb_args, from_tty):
        raise NotImplementedError()
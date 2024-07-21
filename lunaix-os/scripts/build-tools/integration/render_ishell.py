from lcfg.api import (
    RenderContext, 
    InteractiveRenderer, 
    Renderable,
    ConfigTypeCheckError,
    ConfigLoadException
)
from lcfg.common import LConfigEnvironment

import shlex
from lib.utils import join_path
from pathlib import Path
import readline

class ShellException(Exception):
    def __init__(self, *args: object) -> None:
        super().__init__(*args)

class ViewElement:
    def __init__(self, label, node) -> None:
        self.label = label
        self.node = node

    def expand(self, sctx):
        return False

    def read(self, sctx):
        return None
    
    def write(self, sctx, val):
        return None
    
    def get_help(self, sctx):
        return self.node.help_prompt()
    
    def get_type(self, sctx):
        return "N/A"
    
    def draw(self, sctx, canvas):
        pass


class SubviewLink(ViewElement):
    def __init__(self, label, node, cb) -> None:
        super().__init__(label, node)
        self.__callback = cb

    def expand(self, sctx):
        sctx.clear_view()
        self.__callback(sctx)
        return True
    
    def draw(self, sctx, canvas):
        print(f" [{self.label}]")

    def get_type(self, sctx):
        return f"Collection: {self.label}"

class RootWrapper(ViewElement):
    def __init__(self, root) -> None:
        self.__root = root
        super().__init__("root", None)

    def expand(self, sctx):
        sctx.clear_view()
        self.__root.render(sctx)
        return True
    
    def draw(self, sctx, canvas):
        pass

    def get_type(self, sctx):
        return ""

class FieldView(ViewElement):
    def __init__(self, label, node) -> None:
        super().__init__(label, node)

    def read(self, sctx):
        return self.node.get_value()
    
    def write(self, sctx, val):
        if self.node.read_only():
            return None
        
        self.node.set_value(val)
        return val
    
    def get_type(self, sctx):
        return f"Config term: {self.label}\n{self.node.get_type()}"
    
    def draw(self, sctx, canvas):
        suffix = ""
        if self.node.read_only():
            suffix = " (ro)"
        print(f" {self.label}{suffix}")

class ShellContext(RenderContext):
    def __init__(self) -> None:
        super().__init__()

        self._view = {}
        self._subviews = []
        self._field = []
    
    def add_expandable(self, label, node, on_expand_cb):
        name = node.get_name()
        self._view[name] = SubviewLink(name, node, on_expand_cb)
        self._subviews.append(name)
    
    def add_field(self, label, node):
        name = node.get_name()
        self._view[name] = FieldView(name, node)
        self._field.append(name)
    
    def clear_view(self):
        self._view.clear()
        self._subviews.clear()
        self._field.clear()

    def redraw(self):
        for v in self._subviews + self._field:
            self._view[v].draw(self, None)

    def get_view(self, label):
        if label in self._view:
            return self._view[label]
        return None

class InteractiveShell(InteractiveRenderer):
    def __init__(self, root: Renderable) -> None:
        super().__init__()
        self.__levels = [RootWrapper(root)]
        self.__aborted = True
        self.__sctx = ShellContext()
        self.__cmd = {
            "ls": (
                "list all config node under current collection",
                lambda *args: 
                    self.__sctx.redraw()
            ),
            "help": (
                "print help prompt for given name of node",
                lambda *args: 
                    self.print_help(*args)
            ),
            "type": (
                "print the type of a config node",
                lambda *args: 
                    self.print_type(*args)
            ),
            "cd": (
                "navigate to a collection node given a unix-like path",
                lambda *args: 
                    self.get_in(*args)
            ),
            "usage": (
                "print out the usage",
                lambda *args:
                    self.print_usage()
            )
        }

    def get_path(self):
        l = [level.label for level in self.__levels[1:]]
        return f"/{'/'.join(l)}"
    
    def resolve(self, relative):
        ctx = ShellContext()
        p = join_path(self.get_path(), relative)
        ps = Path(p).resolve().parts
        if ps[0] == '/':
            ps = ps[1:]

        node = self.__levels[0]
        levels = [node]
        for part in ps:
            if not node.expand(ctx):
                raise ShellException(f"node is not a collection: {part}")
            
            node = ctx.get_view(part)
            if not node:
                raise ShellException(f"no such node: {part}")

            levels.append(node)
        
        return (node, levels)

    def print_usage(self):
        for cmd, (desc, _) in self.__cmd.items():
            print("\n".join([
                cmd,
                f"   {desc}",
                ""
            ]))

    def print_help(self, node_name):
        view, _ = self.resolve(node_name)
        
        print(view.get_help(self.__sctx))

    def print_type(self, node_name):
        view, _ = self.resolve(node_name)
        
        print(view.get_type(self.__sctx))

    def do_read(self, node_name):
        view, _ = self.resolve(node_name)
        rd_val = view.read(self.__sctx)
        if rd_val is None:
            raise ShellException(f"config node {view.label} is not readable")
    
        print(rd_val)

    def do_write(self, node_name, val):
        view, _ = self.resolve(node_name)
        wr = view.write(self.__sctx, val)
        if not wr:
            raise ShellException(f"config node {view.label} is read only")

        print(f"write: {val}")
        self.do_render()

    def get_in(self, node_name):
        view, lvl = self.resolve(node_name)
        
        if not view.expand(self.__sctx):
            print(f"{node_name} is not a collection")
            return
        
        self.__levels = lvl

    def do_render(self):
        
        curr = self.__levels[-1]        
        curr.expand(self.__sctx)

    def __loop(self):
        prefix = "config: %s> "%(self.__levels[-1].label)

        line = input(prefix)
        args = shlex.split(line)
        if not args:
            return True
        
        cmd = args[0]
        if cmd in self.__cmd:
            self.__cmd[cmd][1](*args[1:])
            return True
        
        if cmd == "exit":
            self.__aborted = False
            return False
        
        if cmd == "abort":
            return False
        
        node = self.resolve(cmd)
        if not node:
            raise ShellException(f"unrecognised command {line}")
        
        if len(args) == 3 and args[1] == '=':
            self.do_write(args[0], args[2])
            return True
        
        if len(args) == 1:
            self.do_read(args[0])
            return True
        
        print(f"unrecognised command {line}")
        return True


    def render_loop(self):
        self.do_render()
        print("\n".join([
            "Interactive LConfig Shell",
            "   type 'usage' to find out how to use",
            "   type 'exit' to end (with saving)",
            "   type 'abort' to abort (without saving)",
            "   type node name directly to read the value",
            "   type 'node = val' to set 'val' to 'node'",
            ""
        ]))
        while True:
            try:
                if not self.__loop():
                    break
            except KeyboardInterrupt:
                break
            except ConfigTypeCheckError as e:
                print(e.args[0])
            except ShellException as e:
                print(e.args[0])

        return not self.__aborted
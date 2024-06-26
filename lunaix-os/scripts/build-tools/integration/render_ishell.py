from lcfg.api import (
    RenderContext, 
    InteractiveRenderer, 
    Renderable
)
from lcfg.common import LConfigEnvironment

import shlex
from lib.utils import join_path
from pathlib import Path

class ViewElement:
    def __init__(self, label, node) -> None:
        self.label = node.get_name()
        self.node = node

    def select(self, sctx):
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

    def select(self, sctx):
        sctx.clear_view()
        self.__callback(sctx)
        return True
    
    def draw(self, sctx, canvas):
        print(f"({self.label})")

    def get_type(self, sctx):
        return f"Collection: {self.label}"

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
            suffix = " [ro]"
        print(f"{self.label}{suffix}")

class ShellContext(RenderContext):
    def __init__(self) -> None:
        super().__init__()

        self._view = {}
        self._subviews = []
        self._field = []
    
    def add_expandable(self, label, node, on_expand_cb):
        name = node.get_name()
        self._view[name] = SubviewLink(label, node, on_expand_cb)
        self._subviews.append(name)
    
    def add_field(self, label, node):
        name = node.get_name()
        self._view[name] = FieldView(label, node)
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
        self.__levels = [root]
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
            "open": (
                "open a collection node",
                lambda *args: 
                    self.get_in(*args)
            ),
            "back": (
                "back to upper level collection",
                lambda *args: 
                    self.get_out()
            ),
            "usage": (
                "print out the usage",
                lambda *args:
                    self.print_usage()
            )
        }

    def get_path(self):
        l = [level.label for level in self.__levels]
        return '/'.join(l)
    
    def resolve(self, relative):
        p = join_path(self.get_path(), relative)
        return Path(p).resolve().parts

    def print_usage(self):
        for cmd, (desc, _) in self.__cmd.items():
            print("\n".join([
                cmd,
                f"   {desc}",
                ""
            ]))

    def print_help(self, node_name):
        view = self.__sctx.get_view(node_name)
        if not view:
            print(f"no config node named {node_name}")
            return
        
        print()
        print(view.get_help(self.__sctx))

    def print_type(self, node_name):
        view = self.__sctx.get_view(node_name)
        if not view:
            print(f"no config node named {node_name}")
            return
        
        print()
        print(view.get_type(self.__sctx))

    def get_in(self, node_name):
        view = self.__sctx.get_view(node_name)
        if not view:
            print(f"no config node named {node_name}")
            return
        
        
        if not view.select(self.__sctx):
            print(f"{node_name} is not a collection")
            return
        
        self.__levels.append(view)

    def get_out(self):
        if len(self.__levels) == 1:
            return
        
        self.__levels.pop()
        self.__sctx.clear_view()
        self.do_render()

    def do_render(self):
        
        curr = self.__levels[-1]
        if len(self.__levels) == 1:
            self.__sctx.clear_view()
            curr.render(self.__sctx)
            return
        
        curr.select(self.__sctx)

    def __loop(self):
        root = self.__levels[-1]
        prefix = "config: %s> "
        if isinstance(root, LConfigEnvironment):
            prefix = prefix%("root")
        else:
            prefix = prefix%(root.node.get_name())

        line = input(prefix)
        args = shlex.split(line)
        if not args:
            return True
        
        cmd = args[0]
        if cmd in self.__cmd:
            self.__cmd[cmd][1](*args[1:])
            return True
        
        if cmd == "exit":
            return False
        
        node = self.__sctx.get_view(cmd)
        if not node:
            print(f"unrecognised command {line}")
            return True
        
        if len(args) == 3 and args[1] == '<':
            wr = node.write(self.__sctx, args[2])
            if not wr:
                print(f"config node {cmd} is read only")
            else:
                self.do_render()
                print(f"write: {args[2]}")
            return True
        
        if len(args) == 1:
            rd_val = node.read(self.__sctx)
            if not rd_val:
                print(f"config node {cmd} is not readable")
            else:
                print(rd_val)
            return True
        
        print(f"unrecognised command {line}")
        return True


    def render_loop(self):
        self.do_render()
        print("\n".join([
            "Interactive LConfig Shell",
            "   type 'usage' to find out how to use",
            ""
        ]))
        while True:
            if not self.__loop():
                break

from lcfg.api import RenderContext
from lcfg.types import (
    PrimitiveType,
    MultipleChoiceType
)

import subprocess
import curses
import textwrap
import integration.libtui as tui
import integration.libmenu as menu

from integration.libtui import ColorScope, TuiColor, Alignment, EventType
from integration.libmenu import Dialogue, ListView, show_dialog

__git_repo_info = None
__tainted = False

def mark_tainted():
    global __tainted
    __tainted = True

def unmark_tainted():
    global __tainted
    __tainted = False

def get_git_hash():
    try:
        hsh = subprocess.check_output([
                    'git', 'rev-parse', '--short', 'HEAD'
                ]).decode('ascii').strip()
        branch = subprocess.check_output([
                    'git', 'branch', '--show-current'
                ]).decode('ascii').strip()
        return f"{branch}@{hsh}"
    except:
        return None
    
def get_git_info():
    global __git_repo_info
    return __git_repo_info
    
def do_save(session):
    show_dialog(session, "Notice", "Configuration saved")
    unmark_tainted()

def do_exit(session):
    global __tainted
    if not __tainted:
        session.schedule(EventType.E_QUIT)
        return
    
    quit = QuitDialogue(session)
    quit.show()

class MainMenuContext(tui.TuiContext):
    def __init__(self, session, view_title):
        super().__init__(session)

        self.__title = view_title
        
        self.__prepare_layout()

    def __prepare_layout(self):
        
        root = tui.TuiPanel(self, "main_panel")
        root.set_size("*-10", "*-5")
        root.set_alignment(Alignment.CENTER)
        root.drop_shadow(1, 2)
        root.border(True)

        layout = tui.FlexLinearLayout(self, "layout", "6,*,5")
        layout.orientation(tui.FlexLinearLayout.PORTRAIT)
        layout.set_size("*", "*")
        layout.set_padding(1, 1, 1, 1)

        listv = ListView(self, "list_view")
        listv.set_size("70", "*")
        listv.set_alignment(Alignment.CENTER)

        hint = tui.TuiTextBlock(self, "hint")
        hint.set_size(w="*")
        hint.set_local_pos("0.1*", 0)
        hint.height_auto_fit(True)
        hint.set_text(
            "Use <UP>/<DOWN>/<ENTER> to select from list\n"
            "Use <TAB>/<RIGHT>/<LEFT> to change focus\n"
            "<H>: show help (if applicable), <BACKSPACE>: back previous level"
        )
        hint.set_alignment(Alignment.CENTER | Alignment.LEFT)

        suffix = ""
        btns_defs = [
            { 
                "text": "Save", 
                "onclick": lambda x: do_save(self.session())
            },
            { 
                "text": "Exit", 
                "onclick": lambda x: do_exit(self.session())
            }
        ]

        repo_info = get_git_info()

        if self.__title:
            suffix += f" - {self.__title}"
            btns_defs.insert(1, { 
                "text": "Back", 
                "onclick": lambda x: self.session().pop_context() 
            })

        btns = menu.create_buttons(self, btns_defs, sizes="50,*")

        layout.set_cell(0, hint)
        layout.set_cell(1, listv)
        layout.set_cell(2, btns)

        t = menu.create_title(self, "Lunaix Kernel Configuration" + suffix)
        t2 = menu.create_title(self, repo_info)
        t2.set_alignment(Alignment.BOT | Alignment.RIGHT)

        root.add(t)
        root.add(t2)
        root.add(layout)

        self.set_root(root)
        self.__menu_list = listv

    def menu(self):
        return self.__menu_list

    def _handle_key_event(self, key):
        if key == curses.KEY_BACKSPACE or key == 8:
            self.session().pop_context()
        elif key == 27:
            do_exit(self.session())
            return
        
        super()._handle_key_event(key)

class ItemType:
    Expandable = 0
    Switch = 1
    Choice = 2
    Other = 3
    def __init__(self, node, expandable) -> None:
        self.__node = node

        if expandable:
            self.__type = ItemType.Expandable
            return
        
        self.__type = ItemType.Other
        self.__primitive = False
        type_provider = node.get_type()

        if isinstance(type_provider, PrimitiveType):
            self.__primitive = True

            if isinstance(type_provider, MultipleChoiceType):
                self.__type = ItemType.Choice
            elif type_provider._type == bool:
                self.__type = ItemType.Switch
        
        self.__provider = type_provider

    def get_formatter(self):
        if self.__type == ItemType.Expandable:
            return "%s ---->"
        
        v = self.__node.get_value()

        if self.is_switch():
            mark = "*" if v else " "
            return f"[{mark}] %s"
        
        if self.is_choice() or isinstance(v, int):
            return f"({v}) %s"
        
        return "%s"
    
    def expandable(self):
        return self.__type == ItemType.Expandable
    
    def is_switch(self):
        return self.__type == ItemType.Switch
    
    def is_choice(self):
        return self.__type == ItemType.Choice
    
    def read_only(self):
        return not self.expandable() and self.__node.read_only()
    
    def provider(self):
        return self.__provider

class MultiChoiceItem(tui.SimpleList.Item):
    def __init__(self, value, get_val) -> None:
        super().__init__()
        self.__val = value
        self.__getval = get_val

    def get_text(self):
        marker = "*" if self.__getval() == self.__val else " "
        return f"  ({marker}) {self.__val}"
    
    def value(self):
        return self.__val
    
class LunaConfigItem(tui.SimpleList.Item):
    def __init__(self, session, node, name, expand_cb = None):
        super().__init__()
        self.__node = node
        self.__type = ItemType(node, expand_cb is not None)
        self.__name = name
        self.__expand_cb = expand_cb
        self.__session = session
    
    def get_text(self):
        fmt = self.__type.get_formatter()
        if self.__type.read_only():
            fmt += "*"
        return f"   {fmt%(self.__name)}"
    
    def on_selected(self):
        if self.__type.read_only():
            show_dialog(
                self.__session, 
                f"Read-only: \"{self.__name}\"", 
                f"Value defined in this field:\n\n'{self.__node.get_value()}'")
            return
        
        if self.__type.expandable():
            view = CollectionView(self.__session, self.__node, self.__name)
            view.set_reloader(self.__expand_cb)
            view.show()
            return
        
        if self.__type.is_switch():
            v = self.__node.get_value()
            self.change_value(not v)
        else:
            dia = ValueEditDialogue(self.__session, self)
            dia.show()

        self.__session.schedule(EventType.E_REDRAW)

    def name(self):
        return self.__name
    
    def node(self):
        return self.__node
    
    def type(self):
        return self.__type
    
    def change_value(self, val):
        try:
            self.__node.set_value(val)
        except:
            show_dialog(
                self.__session, "Invalid value",
                f"Value: '{val}' does not match the type")
            return False
        
        mark_tainted()
        CollectionView.reload_active(self.__session)
        return True
    
    def on_key_pressed(self, key):
        if (key & ~0b100000) != ord('H'):
            return
        
        h = self.__node.help_prompt()
        if not self.__type.expandable():
            h = "\n".join([
                h, "", "--------",
                "Supported Values:",
                textwrap.indent(str(self.__type.provider()), "  ")
            ])

        dia = HelpDialogue(self.__session, f"Help: '{self.__name}'", h)
        dia.show()
    
class CollectionView(RenderContext):
    def __init__(self, session, node, label = None) -> None:
        super().__init__()
        
        ctx = MainMenuContext(session, label)
        self.__node = node
        self.__tui_ctx = ctx
        self.__listv = ctx.menu()
        self.__session = session
        self.__reloader = lambda x: node.render(x)

        ctx.set_state(self)

    def set_reloader(self, cb):
        self.__reloader = cb

    def add_expandable(self, label, node, on_expand_cb):
        item = LunaConfigItem(self.__session, node, label, on_expand_cb)
        self.__listv.add_item(item)

    def add_field(self, label, node):
        item = LunaConfigItem(self.__session, node, label)
        self.__listv.add_item(item)

    def show(self):
        self.reload()
        self.__session.push_context(self.__tui_ctx)

    def reload(self):
        self.__listv.clear()
        self.__reloader(self)
        self.__session.schedule(EventType.E_REDRAW)

    @staticmethod
    def reload_active(session):
        state = session.active().state()
        if isinstance(state, CollectionView):
            state.reload()

class ValueEditDialogue(menu.Dialogue):
    def __init__(self, session, item: LunaConfigItem):
        name = item.name()
        title = f"Edit \"{name}\""
        super().__init__(session, title, None, False, 
                         "Confirm", "Cancle")
        
        self.__item = item
        self.__value = item.node().get_value()

        self.decide_content()

    def __get_val(self):
        return self.__value

    def decide_content(self):
        if not self.__item.type().is_choice():
            self.set_input_dialogue(True)
            return
        
        listv = ListView(self.context(), "choices")
        listv.set_size("0.8*", "*")
        listv.set_alignment(Alignment.CENTER)
        listv.set_onselected_cb(self.__on_selected)

        for t in self.__item.type().provider()._type:
            listv.add_item(MultiChoiceItem(t, self.__get_val))
        
        self.set_content(listv)
        self.set_size()
    
    def __on_selected(self, listv, index, item):
        self.__value = item.value()
    
    def _ok_onclick(self):
        if self._textbox:
            self.__value = self._textbox.get_text()

        if self.__item.change_value(self.__value):
            super()._ok_onclick()

class QuitDialogue(menu.Dialogue):
    def __init__(self, session):
        super().__init__(session, 
                         "Quit ?", "Unsaved changes, sure to quit?", False, 
                         "Quit Anyway", "No", "Save and Quit")
        
    def _ok_onclick(self):
        self.session().schedule(EventType.E_QUIT)

    def _abort_onclick(self):
        unmark_tainted()
        self._ok_onclick()


class HelpDialogue(menu.Dialogue):
    def __init__(self, session, title="", content=""):
        super().__init__(session, title, None, no_btn=None)

        self.__content = content
        self.__scroll_y = 0
        self.set_local_pos(0, -2)

    def prepare(self):
        tb = tui.TuiTextBlock(self._context, "content")
        tb.set_size(w="70")
        tb.set_text(self.__content)
        tb.height_auto_fit(True)
        self.__tb = tb
        
        self.__scroll = tui.TuiScrollable(self._context, "scroll")
        self.__scroll.set_size("65", "*")
        self.__scroll.set_alignment(Alignment.CENTER)
        self.__scroll.set_content(tb)

        self.set_size(w="75")
        self.set_content(self.__scroll)
        
        super().prepare()

    def _handle_key_event(self, key):
        if key == curses.KEY_UP:
            self.__scroll_y = max(self.__scroll_y - 1, 0)
            self.__scroll.set_scrollY(self.__scroll_y)
        elif key == curses.KEY_DOWN:
            y = self.__tb._size.y()
            self.__scroll_y = min(self.__scroll_y + 1, y)
            self.__scroll.set_scrollY(self.__scroll_y)
        super()._handle_key_event(key)

class TerminalSizeCheckFailed(Exception):
    def __init__(self, *args: object) -> None:
        super().__init__(*args)

def main(_, root_node):
    global __git_repo_info

    __git_repo_info = get_git_hash()

    session = tui.TuiSession()

    h, w = session.window_size()
    if h < 30 or w < 85:
        raise TerminalSizeCheckFailed((90, 40), (w, h))

    base_background = TuiColor.white.bright()
    session.set_color(ColorScope.WIN,   
                        TuiColor.black, TuiColor.blue)
    session.set_color(ColorScope.PANEL, 
                        TuiColor.black, base_background)
    session.set_color(ColorScope.TEXT,  
                        TuiColor.black, base_background)
    session.set_color(ColorScope.TEXT_HI,  
                        TuiColor.magenta, base_background)
    session.set_color(ColorScope.SHADOW, 
                        TuiColor.black, TuiColor.black)
    session.set_color(ColorScope.SELECT, 
                        TuiColor.white, TuiColor.black.bright())
    session.set_color(ColorScope.HINT, 
                        TuiColor.cyan, base_background)
    session.set_color(ColorScope.BOX, 
                        TuiColor.black, TuiColor.white)

    main_view = CollectionView(session, root_node)
    main_view.show()

    session.event_loop()

def menuconfig(root_node):
    global __tainted
    curses.wrapper(main, root_node)

    return not __tainted
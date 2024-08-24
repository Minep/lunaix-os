from lcfg.api import RenderContext
from lcfg.types import (
    PrimitiveType,
    MultipleChoiceType
)

import subprocess
import curses
import integration.libtui as tui
import integration.libmenu as menu

from integration.libtui import ColorScope, TuiColor, Alignment, EventType
from integration.libmenu import Dialogue, ListView, show_dialog

__git_repo_info = None

def get_git_hash() -> str:
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

def create_main_menu_context(session, view_title):
    global __git_repo_info

    main_ctx = tui.TuiContext(session)

    root = tui.TuiPanel(main_ctx, "main_panel")
    root.set_size("0.9*", "0.8*")
    root.set_alignment(Alignment.CENTER)
    root.drop_shadow(1, 2)
    root.border(True)

    layout = tui.FlexLinearLayout(main_ctx, "layout", "5,*,5")
    layout.orientation(tui.FlexLinearLayout.PORTRAIT)
    layout.set_size("*", "*")
    layout.set_padding(1, 1, 1, 1)

    listv = ListView(main_ctx, "list_view")
    listv.set_size("80", "*")
    listv.set_alignment(Alignment.CENTER)

    hint = tui.TuiTextBlock(main_ctx, "hint")
    hint.set_size("*", "3")
    hint.set_text(
        "Use <UP>/<DOWN>/<ENTER> to select from list\n"
        "Use <TAB>/<RIGHT>/<LEFT> to change focus\n"
        "   *: Item is read-only"
    )
    hint.set_alignment(Alignment.CENTER | Alignment.LEFT)
    hint.set_margin(0, 0, 0, 10)

    suffix = ""
    btns_defs = [
        { 
            "text": "Save", 
            "onclick": lambda x: show_diag(session) 
        },
        { 
            "text": "Exit", 
            "onclick": lambda x: session.schedule(EventType.E_QUIT)
        }
    ]

    if __git_repo_info:
        suffix += f" - {__git_repo_info}"

    if view_title:
        suffix += f" - {view_title}"
        btns_defs.insert(1, { 
            "text": "Back", 
            "onclick": lambda x: session.pop_context() 
        })

    btns = menu.create_buttons(main_ctx, btns_defs, sizes="50,*")

    layout.set_cell(0, hint)
    layout.set_cell(1, listv)
    layout.set_cell(2, btns)

    t = menu.create_title(main_ctx, "Lunaix Menuconfig" + suffix)
    root.add(t)
    root.add(layout)

    main_ctx.set_root(root)
    return (main_ctx, listv)

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
        if self.__type == ItemType.Switch:
            mark = "*" if v else " "
            return f"[{mark}] %s"
        
        if self.__primitive:
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
                f"Value defined in this field:\n'{self.__node.get_value()}'")
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
                self.session(), "Invalid value",
                f"Value: '{val}' does not match the type")
            return False
        
        CollectionView.reload_active(self.__session)
        return True
    
class CollectionView(RenderContext):
    def __init__(self, session, node, label = None) -> None:
        super().__init__()
        
        ctx, listv = create_main_menu_context(session, label)
        self.__node = node
        self.__tui_ctx = ctx
        self.__listv = listv
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
    
    def __on_selected(self, listv, index, item):
        self.__value = item.value()
    
    def _ok_onclick(self):
        if self._textbox:
            self.__value = self._textbox.get_text()

        if self.__item.change_value(self.__value):
            super()._ok_onclick()

def main(w, root_node):
    global __git_repo_info

    __git_repo_info = get_git_hash()

    session = tui.TuiSession()

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
    curses.wrapper(main, root_node)
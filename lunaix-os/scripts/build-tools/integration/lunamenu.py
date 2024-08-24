import curses
import libtui as tui
import libmenu as menu
from libtui import ColorScope, TuiColor, Alignment, EventType
from libmenu import Dialogue, ListView

class LunaConfigItem(tui.SimpleList.Item):
    def __init__(self, node):
        super().__init__()
        self.__node = node
    
    def get_text(self):
        return f"  {self.__node}"
    
    def on_selected(self):
        pass

def show_diag(session):
    dia = Dialogue(session, title="Test Dialog", content="This is test", input=True)
    dia.show()

def create_main_menu_context(session, node):
    main_ctx = tui.TuiContext(session)

    root = tui.TuiPanel(main_ctx, "main_panel")
    root.set_size("0.8*", "0.8*")
    root.set_alignment(Alignment.CENTER)
    root.drop_shadow(1, 2)
    root.border(True)

    layout = tui.StackLayout(main_ctx, "layout", "5,*,5")
    layout.orientation(tui.StackLayout.PORTRAIT)
    layout.set_size("*", "*")
    layout.set_padding(1, 1, 1, 1)

    listv = ListView(main_ctx, "list_view")
    listv.set_size("0.5*", "*")
    listv.set_alignment(Alignment.CENTER)

    hint = tui.TuiLabel(main_ctx, "hint")
    hint.set_text(
        "Use <UP>/<DOWN>/<ENTER> to select from list, "
        "<TAB>/<RIGHT>/<LEFT> to change focus"
    )
    hint.set_alignment(Alignment.CENTER | Alignment.LEFT)
    hint.set_margin(0, 0, 0, 10)

    for i in range(40):
        listv.add_item(LunaConfigItem(f"Item {i}"))

    btns = menu.create_buttons(main_ctx, [
        { 
            "text": "Save", 
            "onclick": lambda x: show_diag(session) 
        },
        { 
            "text": "Exit", 
            "onclick": lambda x: session.schedule(EventType.E_QUIT) 
        }
    ], sizes="20,*")

    layout.set_cell(0, hint)
    layout.set_cell(1, listv)
    layout.set_cell(2, btns)


    root.add(menu.create_title(main_ctx, "Lunaix Menuconfig"))
    root.add(layout)

    main_ctx.set_root(root)
    return main_ctx

def main(w):
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

    ctx = create_main_menu_context(session, None)

    session.push_context(ctx)
    session.event_loop()

if __name__ == "__main__":
    curses.wrapper(main)

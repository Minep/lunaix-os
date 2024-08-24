import curses
import libtui as tui
from libtui import ColorScope, TuiColor, Alignment, EventType

def create_buttons(main_ctx, btn_defs, sizes = "*,*"):
    size_defs = ",".join(['*'] * len(btn_defs))

    layout = tui.StackLayout(main_ctx, "buttons", size_defs)
    layout.orientation(tui.StackLayout.LANDSCAPE)
    layout.set_size(*(sizes.split(',')[:2]))
    layout.set_padding(1, 1, 1, 1)
    layout.set_alignment(Alignment.CENTER | Alignment.BOT)

    for i, btn_def in enumerate(btn_defs):
        but1 = tui.TuiButton(main_ctx, "b1")
        but1.set_text(btn_def["text"])
        but1.set_click_callback(btn_def["onclick"])
        but1.set_alignment(Alignment.CENTER)
        
        layout.set_cell(i, but1)

    return layout

def create_title(ctx, title):
    _t = tui.TuiLabel(ctx, "label")
    _t.set_text(title)
    _t.set_local_pos(1, 0)
    _t.set_alignment(Alignment.TOP | Alignment.LEFT)
    _t.hightlight(True)
    _t.pad_around(True)
    return _t

class ListView(tui.TuiObject):
    def __init__(self, context, id):
        super().__init__(context, id)

        self.__create_layout()

    def __create_layout(self):
        hint_moveup = tui.TuiLabel(self._context, "movup")
        hint_moveup.override_color(ColorScope.HINT)
        hint_moveup.set_text("^^^ - MORE")
        hint_moveup.set_visbility(False)
        hint_moveup.set_alignment(Alignment.TOP)

        hint_movedown = tui.TuiLabel(self._context, "movdown")
        hint_movedown.override_color(ColorScope.HINT)
        hint_movedown.set_text("vvv - MORE")
        hint_movedown.set_visbility(False)
        hint_movedown.set_alignment(Alignment.BOT)

        list_ = tui.SimpleList(self._context, "list")
        list_.set_size("*", "*")
        list_.set_alignment(Alignment.CENTER | Alignment.TOP)

        scroll = tui.TuiScrollable(self._context, "scroll")
        scroll.set_size("*", "*")
        scroll.set_alignment(Alignment.CENTER)
        scroll.set_content(list_)

        layout = tui.StackLayout(
                    self._context, f"main_layout", "2,*,2")
        layout.set_size("*", "*")
        layout.set_alignment(Alignment.CENTER)
        layout.orientation(tui.StackLayout.PORTRAIT)
        layout.set_parent(self)

        layout.set_cell(0, hint_moveup)
        layout.set_cell(1, scroll)
        layout.set_cell(2, hint_movedown)

        self.__hint_up   = hint_moveup
        self.__hint_down = hint_movedown
        self.__list      = list_
        self.__scroll    = scroll
        self.__layout    = layout

    def add_item(self, item):
        self.__list.add_item(item)

    def on_draw(self):
        super().on_draw()
        
        more_above = not self.__scroll.reached_top()
        more_below = not self.__scroll.reached_last()
        self.__hint_up.set_visbility(more_above)
        self.__hint_down.set_visbility(more_below)

        self.__layout.on_draw()

    def on_layout(self):
        super().on_layout()
        self.__layout.on_layout()

    def on_event(self, ev_type, ev_arg):
        super().on_event(ev_type, ev_arg)
        self.__layout.on_event(ev_type, ev_arg)

        e = EventType.value(ev_type)
        if e != EventType.E_KEY:
            return
        
        if (ev_arg != curses.KEY_DOWN and ev_arg != curses.KEY_UP):
            return

        self.__scroll.set_scrollY(self.__list.index())

class Dialogue(tui.TuiContext):
    Pending = 0
    Yes = 1
    No = 2
    Abort = 3
    def __init__(self, session, title = "", content = "", input=False,
                 ok_btn = "OK", no_btn = "No", abort_btn = None):
        super().__init__(session)

        self.__btns = [
            { "text": ok_btn, "onclick": lambda x: self.__ok_onclick() },
            { "text": no_btn, "onclick": lambda x: self.__no_onclick() }
        ]

        if abort_btn:
            self.__btns.append({ 
                "text": abort_btn, 
                "onclick": lambda x: self.__abort_onclick() 
            })

        self.__title_txt = title
        self.__status = Dialogue.Pending
        self.__content = content
        self.__input_dialog = input

    def prepare(self):
        self.__create_layout(self.__title_txt)
    
    def __ok_onclick(self):
        self.__status = Dialogue.Yes
        self.__close()

    def __no_onclick(self):
        self.__status = Dialogue.No
        self.__close()

    def __abort_onclick(self):
        self.__status = Dialogue.Abort
        self.__close()

    def __create_layout(self, title):
        panel  = tui.TuiPanel(self, "panel")
        panel.set_size("0.5*", "0.5*")
        panel.set_alignment(Alignment.CENTER)
        panel.drop_shadow(1, 2)
        panel.border(True)

        layout = tui.StackLayout(self, "layout", "*,5")
        layout.orientation(tui.StackLayout.PORTRAIT)
        layout.set_size("*", "*")
        layout.set_padding(1, 1, 1, 1)

        btn_grp = create_buttons(self, self.__btns, "0.5*,*")

        content = self.__create_content()

        layout.set_cell(0, content)
        layout.set_cell(1, btn_grp)

        t = create_title(self, title)
        t.set_alignment(Alignment.CENTER | Alignment.TOP)

        panel.add(t)
        panel.add(layout)

        self.set_root(panel)
        self.__title = t
        self.__layout = layout

    def __create_content(self):
        text = None
        if self.__content is not None:
            text = tui.TuiTextBlock(self, "tb")
            text.set_size("0.6*", "10")
            text.set_alignment(Alignment.CENTER)
            text.set_text(self.__content)
        
        if not self.__input_dialog:
            return text
        
        layout = tui.StackLayout(self, "layout", "*,5")
        layout.orientation(tui.StackLayout.PORTRAIT)
        layout.set_size("*", "*")
        
        tb = tui.TuiTextBox(self, "input")
        tb.set_size("0.5*", "3")
        tb.set_alignment(Alignment.CENTER)
        
        if text:
            layout.set_cell(0, text)
        layout.set_cell(1, tb)

        return layout
        
    def __close(self):
        self.session().pop_context()

    def status(self):
        return self.__status
    
    def show(self, title=None):
        if title:
            self.__title.set_text(title)
        self.session().push_context(self)

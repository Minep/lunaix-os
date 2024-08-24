import curses
import integration.libtui as tui
from integration.libtui import ColorScope, TuiColor, Alignment, EventType

def create_buttons(main_ctx, btn_defs, sizes = "*,*"):
    size_defs = ",".join(['*'] * len(btn_defs))

    layout = tui.FlexLinearLayout(main_ctx, "buttons", size_defs)
    layout.orientation(tui.FlexLinearLayout.LANDSCAPE)
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
    _t.set_alignment(Alignment.TOP | Alignment.CENTER)
    _t.hightlight(True)
    _t.pad_around(True)
    return _t

class ListView(tui.TuiObject):
    def __init__(self, context, id):
        super().__init__(context, id)

        self.__create_layout()

        self.__sel_changed = None
        self.__sel = None

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

        list_.set_onselected_cb(self._on_selected)
        list_.set_onselection_change_cb(self._on_sel_changed)

        scroll = tui.TuiScrollable(self._context, "scroll")
        scroll.set_size("*", "*")
        scroll.set_alignment(Alignment.CENTER)
        scroll.set_content(list_)

        layout = tui.FlexLinearLayout(
                    self._context, f"main_layout", "2,*,2")
        layout.set_size("*", "*")
        layout.set_alignment(Alignment.CENTER)
        layout.orientation(tui.FlexLinearLayout.PORTRAIT)
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

    def clear(self):
        self.__list.clear()

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

    def _on_sel_changed(self, listv, prev, new):
        h = self.__scroll._size.y()
        self.__scroll.set_scrollY((new + 1) // h * h)

        if self.__sel_changed:
            self.__sel_changed(listv, prev, new)

    def _on_selected(self, listv, index, item):
        if self.__sel:
            self.__sel(listv, index, item)
    
    def set_onselected_cb(self, cb):
        self.__sel = cb

    def set_onselect_changed_cb(self, cb):
        self.__sel_changed = cb

class Dialogue(tui.TuiContext):
    Pending = 0
    Yes = 1
    No = 2
    Abort = 3
    def __init__(self, session, title = "", content = "", input=False,
                 ok_btn = "OK", no_btn = "No", abort_btn = None):
        super().__init__(session)

        self.__btns = [
            { "text": ok_btn, "onclick": lambda x: self._ok_onclick() }
        ]

        if no_btn:
            self.__btns.append({ 
                "text": no_btn, 
                "onclick": lambda x: self._no_onclick() 
            })
        if abort_btn:
            self.__btns.append({ 
                "text": abort_btn, 
                "onclick": lambda x: self._abort_onclick() 
            })

        self.__title_txt = title
        self.__status = Dialogue.Pending
        self.__content = content
        self.__input_dialog = input
        self._textbox = None

        self.set_size("70", "0.5*")
        self.set_alignment(Alignment.CENTER)

    def set_content(self, content):
        self.__content = content

    def set_input_dialogue(self, yes):
        self.__input_dialog = yes

    def prepare(self):
        self.__create_layout(self.__title_txt)

    def _handle_key_event(self, key):
        if key == 27:
            self.__close()
            return
        super()._handle_key_event(key)
        
    
    def _ok_onclick(self):
        self.__status = Dialogue.Yes
        self.__close()

    def _no_onclick(self):
        self.__status = Dialogue.No
        self.__close()

    def _abort_onclick(self):
        self.__status = Dialogue.Abort
        self.__close()

    def __create_layout(self, title):
        panel  = tui.TuiPanel(self, "panel")
        layout = tui.FlexLinearLayout(self, "layout", "*,3")
        btn_grp = create_buttons(self, self.__btns)
        t = create_title(self, title)
        content = self.__create_content()

        self.__title = t
        self.__layout = layout
        self.__panel = panel

        panel._dyn_size.set(self._dyn_size)
        panel._local_pos.set(self._local_pos)
        panel.set_alignment(self._align)
        panel.drop_shadow(1, 2)
        panel.border(True)

        layout.orientation(tui.FlexLinearLayout.PORTRAIT)
        layout.set_size("*", "*")
        layout.set_padding(4, 1, 1, 1)

        t.set_alignment(Alignment.CENTER | Alignment.TOP)

        layout.set_cell(0, content)
        layout.set_cell(1, btn_grp)

        panel.add(t)
        panel.add(layout)

        self.set_root(panel)

    def __create_content(self):
        text = None
        if isinstance(self.__content, str):
            text = tui.TuiTextBlock(self, "tb")
            text.set_size("0.6*", "0.5*")
            text.set_alignment(Alignment.CENTER)
            text.set_text(self.__content)
        elif self.__content is not None:
            return self.__content
        
        if not self.__input_dialog:
            self.set_size(h = "20")
            return text
        
        tb = tui.TuiTextBox(self, "input")
        tb.set_size("0.5*", "3")
        tb.set_alignment(Alignment.CENTER)
        
        if text:
            layout = tui.FlexLinearLayout(self, "layout", "*,5")
            layout.orientation(tui.FlexLinearLayout.PORTRAIT)
            layout.set_size("*", "*")
            layout.set_cell(0, text)
            layout.set_cell(1, tb)
        else:
            layout = tb
            self.set_size(h = "10")
        
        self.set_curser_mode(1)

        self._textbox = tb

        return layout
        
    def __close(self):
        self.session().pop_context()

    def status(self):
        return self.__status
    
    def show(self, title=None):
        if title:
            self.__title.set_text(title)
        self.session().push_context(self)


def show_dialog(session, title, text):
    dia = Dialogue(session, title=title, content=text, no_btn=None)
    dia.show()
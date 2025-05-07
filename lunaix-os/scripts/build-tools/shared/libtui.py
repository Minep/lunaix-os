#
# libtui - TUI framework using ncurses
#  (c) 2024 Lunaixsky
# 
# I sware, this is the last time I ever touch 
#  any sort of the GUI messes.
#

import curses
import re
import curses.panel as cpanel
import curses.textpad as textpad
import textwrap

def __invoke_fn(obj, fn, *args):
    try:
        fn(*args)
    except:
        _id = obj._id if obj else "<root>"
        raise Exception(_id, str(fn), args)

def resize_safe(obj, co, y, x):
    __invoke_fn(obj, co.resize, y, x)

def move_safe(obj, co, y, x):
    __invoke_fn(obj, co.move, y, x)
    
def addstr_safe(obj, co, y, x, str, *args):
    __invoke_fn(obj, co.addstr, y, x, str, *args)

class _TuiColor:
    def __init__(self, v) -> None:
        self.__v = v
    def __int__(self):
        return self.__v
    def bright(self):
        return self.__v + 8

class TuiColor:
    black     = _TuiColor(curses.COLOR_BLACK)
    red       = _TuiColor(curses.COLOR_RED)
    green     = _TuiColor(curses.COLOR_GREEN)
    yellow    = _TuiColor(curses.COLOR_YELLOW)
    blue      = _TuiColor(curses.COLOR_BLUE)
    magenta   = _TuiColor(curses.COLOR_MAGENTA)
    cyan      = _TuiColor(curses.COLOR_CYAN)
    white     = _TuiColor(curses.COLOR_WHITE)

class Alignment:
    LEFT    = 0b000001
    RIGHT   = 0b000010
    CENTER  = 0b000100
    TOP     = 0b001000
    BOT     = 0b010000
    ABS     = 0b000000
    REL     = 0b100000

class ColorScope:
    WIN       = 1
    PANEL     = 2
    TEXT      = 3
    TEXT_HI   = 4
    SHADOW    = 5
    SELECT    = 6
    HINT      = 7
    BOX       = 8

class EventType:
    E_KEY = 0
    E_REDRAW = 1
    E_QUIT = 2
    E_TASK = 3
    E_CHFOCUS = 4
    E_M_FOCUS = 0b10000000

    def focused_only(t):
        return (t & EventType.E_M_FOCUS)
    
    def value(t):
        return t & ~EventType.E_M_FOCUS
    
    def key_press(t):
        return (t & ~EventType.E_M_FOCUS) == EventType.E_KEY

class Matchers:
    RelSize = re.compile(r"(?P<mult>[0-9]+(?:\.[0-9]+)?)?\*(?P<add>[+-][0-9]+)?")

class BoundExpression:
    def __init__(self, expr = None):
        self._mult = 0
        self._add  = 0

        if expr:
            self.update(expr)

    def set_pair(self, mult, add):
        self._mult = mult
        self._add  = add

    def set(self, expr):
        self._mult = expr._mult
        self._add  = expr._add
        
    def update(self, expr):
        if isinstance(expr, int):
            m = None
        else:
            m = Matchers.RelSize.match(expr)

        if m:
            g = m.groupdict()
            mult = 1 if not g["mult"] else float(g["mult"])
            add = 0 if not g["add"] else int(g["add"])
            self._mult = mult
            self._add  = add
        else:
            self.set_pair(0, int(expr))

    def calc(self, ref_val):
        return int(self._mult * ref_val + self._add)
    
    def absolute(self):
        return self._mult == 0
    
    def nullity(self):
        return self._mult == 0 and self._add == 0
    
    def scale_mult(self, scalar):
        self._mult *= scalar
        return self
    
    @staticmethod
    def normalise(*exprs):
        v = BoundExpression()
        for e in exprs:
            v += e
        return [e.scale_mult(1 / v._mult) for e in exprs]

    def __add__(self, b):
        v = BoundExpression()
        v.set(self)
        v._mult += b._mult
        v._add  += b._add
        return v

    def __sub__(self, b):
        v = BoundExpression()
        v.set(self)
        v._mult -= b._mult
        v._add  -= b._add
        return v
    
    def __iadd__(self, b):
        self._mult += b._mult
        self._add  += b._add
        return self

    def __isub__(self, b):
        self._mult -= b._mult
        self._add  -= b._add
        return self
    
    def __rmul__(self, scalar):
        v = BoundExpression()
        v.set(self)
        v._mult *= scalar
        v._add *= scalar
        return v

    def __truediv__(self, scalar):
        v = BoundExpression()
        v.set(self)
        v._mult /= float(scalar)
        v._add /= scalar
        return v
    
class DynamicBound:
    def __init__(self):
        self.__x = BoundExpression()
        self.__y = BoundExpression()
    
    def dyn_x(self):
        return self.__x

    def dyn_y(self):
        return self.__y
    
    def resolve(self, ref_w, ref_h):
        return (self.__y.calc(ref_h), self.__x.calc(ref_w))
    
    def set(self, dyn_bound):
        self.__x.set(dyn_bound.dyn_x())
        self.__y.set(dyn_bound.dyn_y())

class Bound:
    def __init__(self) -> None:
        self.__x = 0
        self.__y = 0

    def shrinkX(self, dx):
        self.__x -= dx
    def shrinkY(self, dy):
        self.__y -= dy

    def growX(self, dx):
        self.__x += dx
    def growY(self, dy):
        self.__y += dy

    def resetX(self, x):
        self.__x = x
    def resetY(self, y):
        self.__y = y

    def update(self, dynsz, ref_bound):
        y, x = dynsz.resolve(ref_bound.x(), ref_bound.y())
        self.__x = x
        self.__y = y

    def reset(self, x, y):
        self.__x, self.__y = x, y

    def x(self):
        return self.__x
    
    def y(self):
        return self.__y
    
    def yx(self, scale = 1):
        return int(self.__y * scale), int(self.__x * scale)

class TuiStackWindow:
    def __init__(self, obj) -> None:
        self.__obj = obj
        self.__win = curses.newwin(0, 0)
        self.__pan = cpanel.new_panel(self.__win)
        self.__pan.hide()

    def resize(self, h, w):
        resize_safe(self.__obj, self.__win, h, w)
    
    def relocate(self, y, x):
        move_safe(self.__obj, self.__pan, y, x)

    def set_geometric(self, h, w, y, x):
        resize_safe(self.__obj, self.__win, h, w)
        move_safe(self.__obj, self.__pan, y, x)

    def set_background(self, color_scope):
        self.__win.bkgd(' ', curses.color_pair(color_scope))

    def show(self):
        self.__pan.show()

    def hide(self):
        self.__pan.hide()
    
    def send_back(self):
        self.__pan.bottom()

    def send_front(self):
        self.__pan.top()

    def window(self):
        return self.__win

class SpatialObject:
    def __init__(self) -> None:
        self._local_pos = DynamicBound()
        self._pos = Bound()
        self._dyn_size = DynamicBound()
        self._size = Bound()
        self._margin = (0, 0, 0, 0)
        self._padding = (0, 0, 0, 0)
        self._align = Alignment.TOP | Alignment.LEFT

    def set_local_pos(self, x, y):
        self._local_pos.dyn_x().update(x)
        self._local_pos.dyn_y().update(y)

    def set_alignment(self, align):
        self._align = align

    def set_size(self, w = None, h = None):
        if w:
            self._dyn_size.dyn_x().update(w)
        if h:
            self._dyn_size.dyn_y().update(h)

    def set_margin(self, top, right, bottom, left):
        self._margin = (top, right, bottom, left)

    def set_padding(self, top, right, bottom, left):
        self._padding = (top, right, bottom, left)

    def reset(self):
        self._pos.reset(0, 0)
        self._size.reset(0, 0)

    def deduce_spatial(self, constrain):
        self.reset()
        self.__satisfy_bound(constrain)
        self.__satisfy_alignment(constrain)
        self.__satisfy_margin(constrain)
        self.__satisfy_padding(constrain)

        self.__to_corner_pos(constrain)

    def __satisfy_alignment(self, constrain):
        local_pos = self._local_pos
        cbound = constrain._size
        size = self._size

        cy, cx = cbound.yx()
        ry, rx = local_pos.resolve(cx, cy)
        ay, ax = size.yx(0.5)
        
        if self._align & Alignment.CENTER:
            ax = cx // 2
            ay = cy // 2
        
        if self._align & Alignment.BOT:
            ay = min(cy - ay, cy - 1)
            ry = -ry
        elif self._align & Alignment.TOP:
            ay = size.y() // 2

        if self._align & Alignment.RIGHT:
            ax = cx - ax
            rx = -rx
        elif self._align & Alignment.LEFT:
            ax = size.x() // 2

        self._pos.reset(ax + rx, ay + ry)

    def __satisfy_margin(self, constrain):
        tm, lm, bm, rm = self._margin
        
        self._pos.growX(rm - lm)
        self._pos.growY(bm - tm)

    def __satisfy_padding(self, constrain):
        csize = constrain._size
        ch, cw = csize.yx()
        h, w = self._size.yx(0.5)
        y, x = self._pos.yx()

        tp, lp, bp, rp = self._padding

        if not (tp or lp or bp or rp):
            return

        dtp = min(y - h, tp) - tp
        dbp = min(ch - (y + h), bp) - bp

        dlp = min(x - w, lp) - lp
        drp = min(cw - (x + w), rp) - rp

        self._size.growX(drp + dlp)
        self._size.growY(dtp + dbp)

    def __satisfy_bound(self, constrain):
        self._size.update(self._dyn_size, constrain._size)

    def __to_corner_pos(self, constrain):
        h, w = self._size.yx(0.5)
        g_pos = constrain._pos

        self._pos.shrinkX(w)
        self._pos.shrinkY(h)

        self._pos.growX(g_pos.x())
        self._pos.growY(g_pos.y())
        

class TuiObject(SpatialObject):
    def __init__(self, context, id):
        super().__init__()
        self._id = id
        self._context = context
        self._parent = None
        self._visible = True
        self._focused = False

    def set_parent(self, parent):
        self._parent = parent

    def canvas(self):
        if self._parent:
            return self._parent.canvas()
        return (self, self._context.window())
    
    def context(self):
        return self._context
    
    def session(self):
        return self._context.session()

    def on_create(self):
        pass

    def on_destory(self):
        pass

    def on_quit(self):
        pass

    def on_layout(self):
        if self._parent:
            self.deduce_spatial(self._parent)

    def on_draw(self):
        pass

    def on_event(self, ev_type, ev_arg):
        pass

    def on_focused(self):
        self._focused = True

    def on_focus_lost(self):
        self._focused = False

    def set_visbility(self, visible):
        self._visible = visible

    def do_draw(self):
        if self._visible:
            self.on_draw()
    
    def do_layout(self):
        self.on_layout()

class TuiWidget(TuiObject):
    def __init__(self, context, id):
        super().__init__(context, id)
    
    def on_layout(self):
        super().on_layout()
        
        co, _ = self.canvas()

        y, x = co._pos.yx()
        self._pos.shrinkX(x)
        self._pos.shrinkY(y)

class TuiContainerObject(TuiObject):
    def __init__(self, context, id):
        super().__init__(context, id)
        self._children = []

    def add(self, child):
        child.set_parent(self)
        self._children.append(child)

    def children(self):
        return self._children

    def on_create(self):
        super().on_create()
        for child in self._children:
            child.on_create()

    def on_destory(self):
        super().on_destory()
        for child in self._children:
            child.on_destory()
    
    def on_quit(self):
        super().on_quit()
        for child in self._children:
            child.on_quit()

    def on_layout(self):
        super().on_layout()
        for child in self._children:
            child.do_layout()

    def on_draw(self):
        super().on_draw()
        for child in self._children:
            child.do_draw()

    def on_event(self, ev_type, ev_arg):
        super().on_event(ev_type, ev_arg)
        for child in self._children:
            child.on_event(ev_type, ev_arg)


class TuiScrollable(TuiObject):
    def __init__(self, context, id):
        super().__init__(context, id)
        self.__spos = Bound()

        self.__pad = curses.newpad(1, 1)
        self.__pad.bkgd(' ', curses.color_pair(ColorScope.PANEL))
        self.__pad_panel = cpanel.new_panel(self.__pad)
        self.__content = None

    def canvas(self):
        return (self, self.__pad)

    def set_content(self, content):
        self.__content = content
        self.__content.set_parent(self)

    def set_scrollY(self, y):
        self.__spos.resetY(y)

    def set_scrollX(self, x):
        self.__spos.resetX(x)

    def reached_last(self):
        off = self.__spos.y() + self._size.y()
        return off >= self.__content._size.y()
    
    def reached_top(self):
        return self.__spos.y() < self._size.y()

    def on_layout(self):
        super().on_layout()

        if not self.__content:
            return
        
        self.__content.on_layout()
                
        h, w = self._size.yx()
        ch, cw = self.__content._size.yx()
        sh, sw = max(ch, h), max(cw, w)

        self.__spos.resetX(min(self.__spos.x(), max(cw, w) - w))
        self.__spos.resetY(min(self.__spos.y(), max(ch, h) - h))

        resize_safe(self, self.__pad, sh, sw)

    def on_draw(self):
        if not self.__content:
            return
        
        self.__pad_panel.top()
        self.__content.on_draw()

        wminy, wminx = self._pos.yx()
        wmaxy, wmaxx = self._size.yx()
        wmaxy, wmaxx = wmaxy + wminy, wmaxx + wminx

        self.__pad.refresh(*self.__spos.yx(), 
                            wminy, wminx, wmaxy - 1, wmaxx - 1)


class Layout(TuiContainerObject):
    class Cell(TuiObject):
        def __init__(self, context):
            super().__init__(context, "cell")
            self.__obj = None

        def set_obj(self, obj):
            self.__obj = obj
            self.__obj.set_parent(self)

        def on_create(self):
            if self.__obj:
                self.__obj.on_create()

        def on_destory(self):
            if self.__obj:
                self.__obj.on_destory()

        def on_quit(self):
            if self.__obj:
                self.__obj.on_quit()

        def on_layout(self):
            super().on_layout()
            if self.__obj:
                self.__obj.do_layout()

        def on_draw(self):
            if self.__obj:
                self.__obj.do_draw()

        def on_event(self, ev_type, ev_arg):
            if self.__obj:
                self.__obj.on_event(ev_type, ev_arg)
    
    def __init__(self, context, id, ratios):
        super().__init__(context, id)

        rs = [BoundExpression(r) for r in ratios.split(',')]
        self._rs = BoundExpression.normalise(*rs)

        for _ in range(len(self._rs)):
            cell = Layout.Cell(self._context)
            super().add(cell)

        self._adjust_to_fit()

    def _adjust_to_fit(self):
        pass

    def add(self, child):
        raise RuntimeError("invalid operation")
    
    def set_cell(self, i, obj):
        if i > len(self._children):
            raise ValueError(f"cell #{i} out of bound")
        
        self._children[i].set_obj(obj)


class FlexLinearLayout(Layout):
    LANDSCAPE = 0
    PORTRAIT = 1
    def __init__(self, context, id, ratios):
        self.__horizontal = False

        super().__init__(context, id, ratios)

    def orientation(self, orient):
        self.__horizontal = orient == FlexLinearLayout.LANDSCAPE
    
    def on_layout(self):
        self.__apply_ratio()
        super().on_layout()
    
    def _adjust_to_fit(self):
        sum_abs = BoundExpression()
        i = 0
        for r in self._rs:
            if r.absolute():
                sum_abs += r
            else:
                i += 1

        sum_abs /= i
        for i, r in enumerate(self._rs):
            if not r.absolute():
                self._rs[i] -= sum_abs

    def __apply_ratio(self):
        if self.__horizontal:
            self.__adjust_horizontal()
        else:
            self.__adjust_vertical()
        
    def __adjust_horizontal(self):
        acc = BoundExpression()
        for r, cell in zip(self._rs, self.children()):
            cell._dyn_size.dyn_y().set_pair(1, 0)
            cell._dyn_size.dyn_x().set(r)

            cell.set_alignment(Alignment.LEFT)
            cell._local_pos.dyn_y().set_pair(0, 0)
            cell._local_pos.dyn_x().set(acc)

            acc += r

    def __adjust_vertical(self):
        acc = BoundExpression()
        for r, cell in zip(self._rs, self.children()):
            cell._dyn_size.dyn_x().set_pair(1, 0)
            cell._dyn_size.dyn_y().set(r)

            cell.set_alignment(Alignment.TOP | Alignment.CENTER)
            cell._local_pos.dyn_x().set_pair(0, 0)
            cell._local_pos.dyn_y().set(acc)

            acc += r


class TuiPanel(TuiContainerObject):
    def __init__(self, context, id):
        super().__init__(context, id)

        self.__use_border = False
        self.__use_shadow = False
        self.__shadow_param = (0, 0)

        self.__swin = TuiStackWindow(self)
        self.__shad = TuiStackWindow(self)

        self.__swin.set_background(ColorScope.PANEL)
        self.__shad.set_background(ColorScope.SHADOW)

    def canvas(self):
        return (self, self.__swin.window())

    def drop_shadow(self, off_y, off_x):
        self.__shadow_param = (off_y, off_x)
        self.__use_shadow = not (off_y == off_x and off_y == 0)

    def border(self, _b):
        self.__use_border = _b

    def bkgd_override(self, scope):
        self.__swin.set_background(scope)

    def on_layout(self):
        super().on_layout()

        self.__swin.hide()

        h, w = self._size.y(), self._size.x()
        y, x = self._pos.y(), self._pos.x()
        self.__swin.set_geometric(h, w, y, x)
        
        if self.__use_shadow:
            sy, sx = self.__shadow_param
            self.__shad.set_geometric(h, w, y + sy, x + sx)

    def on_destory(self):
        super().on_destory()
        self.__swin.hide()
        self.__shad.hide()

    def on_draw(self):
        win = self.__swin.window()
        win.noutrefresh()

        if self.__use_border:
            win.border()
        
        if self.__use_shadow:
            self.__shad.show()
        else:
            self.__shad.hide()

        self.__swin.show()
        self.__swin.send_front()

        super().on_draw()

class TuiLabel(TuiWidget):
    def __init__(self, context, id):
        super().__init__(context, id)
        self._text = "TuiLabel"
        self._wrapped = []

        self.__auto_fit = True
        self.__trunc = False
        self.__dopad = False
        self.__highlight = False
        self.__color_scope = -1

    def __try_fit_text(self, txt):
        if self.__auto_fit:
            self._dyn_size.dyn_x().set_pair(0, len(txt))
            self._dyn_size.dyn_y().set_pair(0, 1)

    def __pad_text(self):
        for i, t in enumerate(self._wrapped):
            self._wrapped[i] = str.rjust(t, self._size.x())
    
    def set_text(self, text):
        self._text = text
        self.__try_fit_text(text)

    def override_color(self, color = -1):
        self.__color_scope = color

    def auto_fit(self, _b):
        self.__auto_fit = _b

    def truncate(self, _b):
        self.__trunc = _b

    def hightlight(self, _b):
        self.__highlight = _b

    def pad_around(self, _b):
        self.__dopad = _b

    def on_layout(self):
        txt = self._text
        if self.__dopad:
            txt = f" {txt} "
            self.__try_fit_text(txt)
        
        super().on_layout()

        if len(txt) <= self._size.x():
            self._wrapped = [txt]
            self.__pad_text()
            return

        if not self.__trunc:
            txt = txt[:self._size.x() - 1]
            self._wrapped = [txt]
            self.__pad_text()
            return

        self._wrapped = textwrap.wrap(txt, self._size.x())
        self.__pad_text()

    def on_draw(self):
        _, win = self.canvas()
        y, x = self._pos.yx()
        
        if self.__color_scope != -1:
            color = curses.color_pair(self.__color_scope)
        elif self.__highlight:
            color = curses.color_pair(ColorScope.TEXT_HI)
        else:
            color = curses.color_pair(ColorScope.TEXT)

        for i, t in enumerate(self._wrapped):
            addstr_safe(self, win, y + i, x, t, color)


class TuiTextBlock(TuiWidget):
    def __init__(self, context, id):
        super().__init__(context, id)
        self.__lines = []
        self.__wrapped = []
        self.__fit_to_height = False
    
    def set_text(self, text):
        text = textwrap.dedent(text)
        self.__lines = text.split('\n')
        if self.__fit_to_height:
            self._dyn_size.dyn_y().set_pair(0, 0)

    def height_auto_fit(self, yes):
        self.__fit_to_height = yes

    def on_layout(self):
        super().on_layout()

        self.__wrapped.clear()
        for t in self.__lines:
            if not t:
                self.__wrapped.append(t)
                continue
            wrap = textwrap.wrap(t, self._size.x())
            self.__wrapped += wrap

        if self._dyn_size.dyn_y().nullity():
            h = len(self.__wrapped)
            self._dyn_size.dyn_y().set_pair(0, h)

            # redo layouting
            super().on_layout()

    def on_draw(self):
        _, win = self.canvas()
        y, x = self._pos.yx()
        
        color = curses.color_pair(ColorScope.TEXT)
        for i, t in enumerate(self.__wrapped):
            addstr_safe(self, win, y + i, x, t, color)


class TuiTextBox(TuiWidget):
    def __init__(self, context, id):
        super().__init__(context, id)
        self.__box = TuiStackWindow(self)
        self.__box.set_background(ColorScope.PANEL)
        self.__textb = textpad.Textbox(self.__box.window(), True)
        self.__textb.stripspaces = True
        self.__str = ""
        self.__scheduled_edit = False

        self._context.focus_group().register(self, 0)

    def __validate(self, x):
        if x == 10 or x == 9:
            return 7
        return x
    
    def on_layout(self):
        super().on_layout()

        co, _ = self.canvas()
        h, w = self._size.yx()
        y, x = self._pos.yx()
        cy, cx = co._pos.yx()
        y, x = y + cy, x + cx

        self.__box.hide()
        self.__box.set_geometric(1, w - 1, y + h // 2, x + 1)

    def on_draw(self):
        self.__box.show()
        self.__box.send_front()
        
        _, cwin = self.canvas()

        h, w = self._size.yx()
        y, x = self._pos.yx()
        textpad.rectangle(cwin, y, x, y + h - 1, x+w)

        win = self.__box.window()
        win.touchwin()

    def __edit(self):
        self.__str = self.__textb.edit(lambda x: self.__validate(x))
        self.session().schedule(EventType.E_CHFOCUS)
        self.__scheduled_edit = False

    def get_text(self):
        return self.__str
    
    def on_focused(self):
        self.__box.set_background(ColorScope.BOX)
        if not self.__scheduled_edit:
            # edit will block, defer to next update cycle
            self.session().schedule_task(self.__edit)
            self.__scheduled_edit = True

    def on_focus_lost(self):
        self.__box.set_background(ColorScope.PANEL)
        

class SimpleList(TuiWidget):
    class Item:
        def __init__(self) -> None:
            pass
        def get_text(self):
            return "list_item"
        def on_selected(self):
            pass
        def on_key_pressed(self, key):
            pass

    def __init__(self, context, id):
        super().__init__(context, id)
        self.__items = []
        self.__selected = 0
        self.__on_sel_confirm_cb = None
        self.__on_sel_change_cb = None

        self._context.focus_group().register(self)

    def set_onselected_cb(self, cb):
        self.__on_sel_confirm_cb = cb

    def set_onselection_change_cb(self, cb):
        self.__on_sel_change_cb = cb

    def count(self):
        return len(self.__items)
    
    def index(self):
        return self.__selected

    def add_item(self, item):
        self.__items.append(item)

    def clear(self):
        self.__items.clear()

    def on_layout(self):
        super().on_layout()
        self.__selected = min(self.__selected, len(self.__items))
        self._size.resetY(len(self.__items) + 1)

    def on_draw(self):
        _, win = self.canvas()
        w = self._size.x()

        for i, item in enumerate(self.__items):
            color = curses.color_pair(ColorScope.TEXT)
            if i == self.__selected:
                if self._focused:
                    color = curses.color_pair(ColorScope.SELECT)
                else:
                    color = curses.color_pair(ColorScope.BOX)
            
            txt = str.ljust(item.get_text(), w)
            txt = txt[:w]
            addstr_safe(self, win, i, 0, txt, color)

    def on_event(self, ev_type, ev_arg):
        if not EventType.key_press(ev_type):
            return
        
        if len(self.__items) == 0:
            return

        sel = self.__items[self.__selected]

        if ev_arg == 10:
            sel.on_selected()
            
            if self.__on_sel_confirm_cb:
                self.__on_sel_confirm_cb(self, self.__selected, sel)
            return
        
        sel.on_key_pressed(ev_arg)
        
        if (ev_arg != curses.KEY_DOWN and 
            ev_arg != curses.KEY_UP):
            return

        prev = self.__selected
        if ev_arg == curses.KEY_DOWN:
            self.__selected += 1
        else:
            self.__selected -= 1

        self.__selected = max(self.__selected, 0)
        self.__selected = self.__selected % len(self.__items)
        
        if self.__on_sel_change_cb:
            self.__on_sel_change_cb(self, prev, self.__selected)


class TuiButton(TuiLabel):
    def __init__(self, context, id):
        super().__init__(context, id)
        self.__onclick = None

        context.focus_group().register(self)

    def set_text(self, text):
        return super().set_text(f"<{text}>")

    def set_click_callback(self, cb):
        self.__onclick = cb

    def hightlight(self, _b):
        raise NotImplemented()
    
    def on_draw(self):
        _, win = self.canvas()
        y, x = self._pos.yx()
        
        if self._focused:
            color = curses.color_pair(ColorScope.SELECT)
        else:
            color = curses.color_pair(ColorScope.TEXT)

        addstr_safe(self, win, y, x, self._wrapped[0], color)
    
    def on_event(self, ev_type, ev_arg):
        if not EventType.focused_only(ev_type):
            return
        if not EventType.key_press(ev_type):
            return
        
        if ev_arg == ord('\n') and self.__onclick:
            self.__onclick(self)


class TuiSession:
    def __init__(self) -> None:
        self.stdsc = curses.initscr()
        curses.start_color()

        curses.noecho()
        curses.cbreak()

        self.__context_stack = []
        self.__sched_events = []

        ws = self.window_size()
        self.__win = curses.newwin(*ws)
        self.__winbg = curses.newwin(*ws)
        self.__panbg = cpanel.new_panel(self.__winbg)

        self.__winbg.bkgd(' ', curses.color_pair(ColorScope.WIN))

        self.__win.timeout(50)
        self.__win.keypad(True)

    def window_size(self):
        return self.stdsc.getmaxyx()

    def set_color(self, scope, fg, bg):
        curses.init_pair(scope, int(fg), int(bg))

    def schedule_redraw(self):
        self.schedule(EventType.E_REDRAW)

    def schedule_task(self, task):
        self.schedule(EventType.E_REDRAW)
        self.schedule(EventType.E_TASK, task)

    def schedule(self, event, arg = None):
        if len(self.__sched_events) > 0:
            if self.__sched_events[-1] == event:
                return
    
        self.__sched_events.append((event, arg))

    def push_context(self, tuictx):
        tuictx.prepare()
        self.__context_stack.append(tuictx)
        self.schedule(EventType.E_REDRAW)

        curses.curs_set(self.active().curser_mode())

    def pop_context(self):
        if len(self.__context_stack) == 1:
            return
        
        ctx = self.__context_stack.pop()
        ctx.on_destory()
        self.schedule(EventType.E_REDRAW)

        curses.curs_set(self.active().curser_mode())

    def active(self):
        return self.__context_stack[-1]
    
    def event_loop(self):
        if len(self.__context_stack) == 0:
            raise RuntimeError("no tui context to display")
        
        while True:
            key = self.__win.getch()
            if key != -1:
                self.schedule(EventType.E_KEY, key)
            
            if len(self.__sched_events) == 0:
                continue

            evt, arg = self.__sched_events.pop(0)
            if evt == EventType.E_REDRAW:
                self.__redraw()
            elif evt == EventType.E_QUIT:
                self.__notify_quit()
                break

            self.active().dispatch_event(evt, arg)
        
    def __notify_quit(self):
        while len(self.__context_stack) == 0:
            ctx = self.__context_stack.pop()
            ctx.dispatch_event(EventType.E_QUIT, None)

    def __redraw(self):
        self.__win.noutrefresh()
        
        self.active().redraw(self.__win)

        self.__panbg.bottom()

        cpanel.update_panels()
        curses.doupdate()

class TuiFocusGroup:
    def __init__(self) -> None:
        self.__grp = []
        self.__id = 0
        self.__sel = 0
        self.__focused = None
    
    def register(self, tui_obj, pos=-1):
        if pos == -1:
            self.__grp.append((self.__id, tui_obj))
        else:
            self.__grp.insert(pos, (self.__id, tui_obj))
        self.__id += 1
        return self.__id - 1

    def navigate_focus(self, dir = 1):
        self.__sel = (self.__sel + dir) % len(self.__grp)
        f = None if not len(self.__grp) else self.__grp[self.__sel][1]
        if f and f != self.__focused:
            if self.__focused:
                self.__focused.on_focus_lost()
            f.on_focused()
        self.__focused = f

    def focused(self):
        return self.__focused

class TuiContext(TuiObject):
    def __init__(self, session: TuiSession):
        super().__init__(self, "context")
        self.__root = None
        self.__sobj = None
        self.__session = session

        self.__win = None

        y, x = self.__session.window_size()
        self._size.reset(x, y)
        self.set_parent(None)

        self.__focus_group = TuiFocusGroup()
        self.__curser_mode = 0

    def set_curser_mode(self, mode):
        self.__curser_mode = mode

    def curser_mode(self):
        return self.__curser_mode

    def set_root(self, root):
        self.__root = root
        self.__root.set_parent(self)
    
    def set_state(self, obj):
        self.__sobj = obj

    def state(self):
        return self.__sobj

    def prepare(self):
        self.__root.on_create()

    def on_destory(self):
        self.__root.on_destory()

    def canvas(self):
        return (self, self.__win)
    
    def session(self):
        return self.__session
    
    def dispatch_event(self, evt, arg):
        if evt == EventType.E_REDRAW:
            self.__focus_group.navigate_focus(0)
        elif evt == EventType.E_CHFOCUS:
            self.__focus_group.navigate_focus(1)
            self.__session.schedule(EventType.E_REDRAW)
            return
        elif evt == EventType.E_TASK:
            arg()
        elif evt == EventType.E_QUIT:
            self.__root.on_quit()
        elif evt == EventType.E_KEY:
            self._handle_key_event(arg)
        else:
            self.__root.on_event(evt, arg)

        focused = self.__focus_group.focused()
        if focused:
            focused.on_event(evt | EventType.E_M_FOCUS, arg)

    def redraw(self, win):
        self.__win = win
        self.on_layout()
        self.on_draw()

    def on_layout(self):
        self.__root.on_layout()

    def on_draw(self):
        self.__root.on_draw()

    def focus_group(self):
        return self.__focus_group
    
    def _handle_key_event(self, key):
        if key == ord('\t') or key == curses.KEY_RIGHT:
            self.__focus_group.navigate_focus()
        elif key == curses.KEY_LEFT:
            self.__focus_group.navigate_focus(-1)
        else:
            self.__root.on_event(EventType.E_KEY, key)
        
        if self.__focus_group.focused():
            self.__session.schedule(EventType.E_REDRAW)
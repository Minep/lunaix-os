import curses
import re
import time
import curses.panel as cpanel
import textwrap

def resize_safe(obj, co, y, x):
    try:
        co.resize(y, x)
    except:
        raise Exception(obj._id, f"resize {co}", (y, x))

def move_safe(obj, co, y, x):
    try:
        co.move(y, x)
    except:
        raise Exception(obj._id, f"move {co}", (y, x))

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

class SizeMode:
    S_ABS = 1
    S_REL = 2

class EventType:
    E_KEY = 0
    E_REDRAW = 1
    E_QUIT = 2
    E_M_FOCUS = 0b10000000

    def focused_only(t):
        return (t & EventType.E_M_FOCUS)
    
    def value(t):
        return t & ~EventType.E_M_FOCUS

class Matchers:
    RelSize = re.compile(r"(?P<mult>[0-9]+(?:\.[0-9]+)?)?\*(?P<add>[+-][0-9]+)?")

class BoundExpression:
    def __init__(self, expr = None):
        #self._mode = SizeMode.S_ABS
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
    
class SpatialObject:
    def __init__(self) -> None:
        self._local_pos = DynamicBound()
        self._pos = Bound()
        self._dyn_size = DynamicBound()
        self._size = Bound()
        self._margin = (0, 0, 0, 0)
        self._padding = (0, 0, 0, 0)
        self._align = Alignment.ABS

    def set_local_pos(self, x, y):
        self._local_pos.dyn_x().update(x)
        self._local_pos.dyn_y().update(y)

    def set_alignment(self, align):
        self._align = align

    def set_size(self, w, h):
        self._dyn_size.dyn_x().update(w)
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
            ay = cy - ay
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
        
        self._pos.growX(lm - rm)
        self._pos.growY(tm - bm)

    def __satisfy_padding(self, constrain):
        csize = constrain._size
        ch, cw = csize.yx()
        h, w = self._size.yx(0.5)
        y, x = self._pos.yx()

        tp, lp, bp, rp = self._padding

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

    def set_parent(self, parent):
        self._parent = parent

    def canvas(self):
        if self._parent:
            return self._parent.canvas()
        return (self, self._context.window())
    
    def context(self):
        return self._context

    def on_create(self):
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
        pass

    def on_focus_lost(self):
        pass

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
    
    def on_quit(self):
        super().on_quit()
        for child in self._children:
            child.on_quit()

    def on_layout(self):
        super().on_layout()
        for child in self._children:
            child.on_layout()

    def on_draw(self):
        super().on_draw()
        for child in self._children:
            child.on_draw()

    def on_event(self, ev_type, ev_arg):
        super().on_event(ev_type, ev_arg)
        for child in self._children:
            child.on_event(ev_type, ev_arg)


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

        def on_quit(self):
            if self.__obj:
                self.__obj.on_quit()

        def on_layout(self):
            super().on_layout()
            if self.__obj:
                self.__obj.on_layout()

        def on_draw(self):
            if self.__obj:
                self.__obj.on_draw()

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


class StackLayout(Layout):
    LANDSCAPE = 0
    PORTRAIT = 1
    def __init__(self, context, id, ratios):
        self.__horizontal = False

        super().__init__(context, id, ratios)

    def orientation(self, orient):
        self.__horizontal = orient == StackLayout.LANDSCAPE
    
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

        self.__win = curses.newwin(0, 0)
        self.__win.bkgd(' ', curses.color_pair(ColorScope.PANEL))
        self.__panel = cpanel.new_panel(self.__win)

        self.__winsh = curses.newwin(0, 0)
        self.__winsh.bkgd(' ', curses.color_pair(ColorScope.SHADOW))
        self.__panelsh = cpanel.new_panel(self.__winsh)

        self.__panel.hide()
        self.__panelsh.hide()

    def canvas(self):
        return (self, self.__win)

    def drop_shadow(self, off_y, off_x):
        self.__shadow_param = (off_y, off_x)
        self.__use_shadow = not (off_y == off_x and off_y == 0)

    def border(self, _b):
        self.__use_border = _b

    def on_layout(self):
        super().on_layout()

        self.__panel.hide()

        h, w = self._size.y(), self._size.x()
        y, x = self._pos.y(), self._pos.x()
        resize_safe(self, self.__win, h, w)
        move_safe(self, self.__panel, y, x)
        
        if self.__use_shadow:
            sy, sx = self.__shadow_param
            resize_safe(self, self.__winsh, h, w)
            move_safe(self, self.__panelsh, y + sy, x + sx)

    def on_draw(self):
        self.__win.erase()

        if self.__use_border:
            self.__win.border()
        
        if self.__use_shadow:
            self.__panelsh.show()
            self.__winsh.refresh()
        else:
            self.__panelsh.hide()

        self.__panel.top()
        super().on_draw()

        self.__panel.show()
        self.__win.touchwin()
        self.__win.refresh()

class TuiLabel(TuiWidget):
    def __init__(self, context, id):
        super().__init__(context, id)
        self._text = "TuiLabel"
        self._wrapped = []

        self.__auto_fit = True
        self.__wrap = False
        self.__dopad = False
        self.__highlight = False

    def __try_fit_text(self, txt):
        if self.__auto_fit:
            self._dyn_size.dyn_x().set_pair(0, len(txt))
            self._dyn_size.dyn_y().set_pair(0, 1)
    
    def set_text(self, text):
        self._text = text
        self.__try_fit_text(text)

    def auto_fit(self, _b):
        self.__auto_fit = _b

    def wrap_text(self, _b):
        self.__wrap = _b

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
            return

        if not self.__wrap:
            txt = txt[:self._size.x() - 1]
            self._wrapped = [txt]
            return

        self._wrapped = textwrap.wrap(txt, self._size.x())

    def on_draw(self):
        _, win = self.canvas()
        y, x = self._pos.yx()
        
        if self.__highlight:
            color = curses.color_pair(ColorScope.TEXT_HI)
        else:
            color = curses.color_pair(ColorScope.TEXT)

        for i, t in enumerate(self._wrapped):
            win.addstr(y + i, x, t, color)


class TuiButton(TuiLabel):
    def __init__(self, context, id):
        super().__init__(context, id)
        self.__selected = False
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
        
        if self.__selected:
            color = curses.color_pair(ColorScope.SELECT)
        else:
            color = curses.color_pair(ColorScope.TEXT)

        win.addstr(y, x, self._wrapped[0], color)
    
    def on_focused(self):
        self.__selected = True
    
    def on_focus_lost(self):
        self.__selected = False
    
    def on_event(self, ev_type, ev_arg):
        if not EventType.focused_only(ev_type):
            return
        if EventType.value(ev_type) != EventType.E_KEY:
            return
        
        if ev_arg == ord('\n') and self.__onclick:
            self.__onclick(self)


class TuiSession:
    def __init__(self) -> None:
        self.stdsc = curses.initscr()
        curses.start_color()

        curses.noecho()

        self.__context_stack = []
        self.__sched_events = []

    def window_size(self):
        return self.stdsc.getmaxyx()

    def set_color(self, scope, fg, bg):
        curses.init_pair(scope, int(fg), int(bg))

    def schedule(self, event, arg = None):
        self.__sched_events.append((event, arg))

    def push_context(self, tuictx):
        self.__context_stack.append(tuictx)
        self.schedule(EventType.E_REDRAW)

    def pop_context(self):
        if len(self.__context_stack) == 1:
            return
        
        self.__context_stack.pop()
        self.schedule(EventType.E_REDRAW)
    
    def event_loop(self):
        if len(self.__context_stack) == 0:
            raise RuntimeError("no tui context to display")
        
        while True:
            acting = self.__context_stack[-1]
            
            key = acting.window().getch()
            if key != -1:
                self.schedule(EventType.E_KEY, key)
            
            for evt, arg in self.__sched_events:
                if evt == EventType.E_QUIT:
                    self.__notify_quit()
                    break
                acting.dispatch_event(evt, arg)

            self.__sched_events.clear()
        
    def __notify_quit(self):
        while len(self.__context_stack) == 0:
            ctx = self.__context_stack.pop()
            ctx.dispatch_event(EventType.E_QUIT, None)

class TuiFocusGroup:
    def __init__(self) -> None:
        self.__grp = []
        self.__id = 0
        self.__sel = 0
        self.__focused = None
    
    def register(self, tui_obj):
        self.__grp.append((self.__id, tui_obj))
        self.__id += 1
        return self.__id - 1

    def navigate_focus(self, dir = 1):
        if self.__focused:
            self.__focused.on_focus_lost()
        
        self.__sel = (self.__sel + dir) % len(self.__grp)
        f = None if not len(self.__grp) else self.__grp[self.__sel][1]
        if f:
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

        ws = self.__session.window_size()
        self.__win = curses.newwin(*ws)
        self.__winbg = curses.newwin(*ws)
        self.__panbg = cpanel.new_panel(self.__winbg)

        self.__winbg.bkgd(' ', curses.color_pair(ColorScope.WIN))

        self.__win.timeout(100)
        self.__win.keypad(True)

        y, x = ws
        self._size.reset(x, y)
        self.set_parent(None)

        self.__focus_group = TuiFocusGroup()

    def set_root(self, root):
        self.__root = root
        self.__root.set_parent(self)
    
    def set_state(self, obj):
        self.__sobj = obj

    def state(self):
        return self.__sobj

    def prepare(self):
        self.__root.on_create()

    def window(self):
        return self.__win
    
    def session(self):
        return self.__session
    
    def dispatch_event(self, evt, arg):
        if evt == EventType.E_REDRAW:
            self.redraw()
        elif evt == EventType.E_QUIT:
            self.__root.on_quit()
        elif evt == EventType.E_KEY:
            self.__handle_key_event(arg)
        else:
            self.__root.on_event(evt, arg)

        focused = self.__focus_group.focused()
        if focused:
            focused.on_event(evt | EventType.E_M_FOCUS, arg)

    def redraw(self):
        self.__session.stdsc.erase()
        self.__win.erase()

        self.__root.on_layout()
        self.__root.on_draw()

        self.__panbg.bottom()
        self.__win.touchwin()
        self.__win.refresh()

        cpanel.update_panels()
        curses.doupdate()

    def on_layout(self):
        pass

    def focus_group(self):
        return self.__focus_group
    
    def __handle_key_event(self, key):
        if key == ord('\t') or key == curses.KEY_RIGHT:
            self.__focus_group.navigate_focus()
        elif key == curses.KEY_LEFT:
            self.__focus_group.navigate_focus(-1)
        else:
            self.__root.on_event(EventType.E_KEY, key)
            return
        
        if self.__focus_group.focused():
            self.__session.schedule(EventType.E_REDRAW)
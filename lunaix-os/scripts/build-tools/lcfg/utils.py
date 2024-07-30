import ast

def extract_decorators(fn_ast: ast.FunctionDef, fname = None, only_name=False):
    decors = fn_ast.decorator_list
    results = []
    for decor in decors:
        _args = []
        _kwargs = []
        if isinstance(decor, ast.Name):
            name = decor.id
        elif isinstance(decor, ast.Call):
            if not isinstance(decor.func, ast.Name):
                continue
            name = decor.func.id
            _args = decor.args
            _kwargs = decor.keywords
        else:
            continue

        if fname and name != fname:
            continue

        if only_name:
            results.append(name)
            continue
        
        unpacked = []
        kwargs = {}
        for arg in _args:
            if isinstance(arg, ast.Constant):
                unpacked.append(arg.value)
        
        for kwarg in _kwargs:
            if isinstance(kwarg.value, ast.Constant):
                kwargs[kwarg.arg] = kwarg.value.value
        
        results.append((name, unpacked, kwargs))

    return results

def to_displayable(name):
    l = name.strip().split('_')
    for i, s in enumerate(l):
        l[i] = str.upper(s[0]) + s[1:]
    return " ".join(l)

def is_primitive(val):
    return val in [int, str, bool]

def is_basic(val):
    basic = [int, str, bool]
    return (
        val in basic or
        any([isinstance(val, x) for x in basic])
    )
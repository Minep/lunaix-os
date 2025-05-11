# LunaConfig

LunaConfig is a configuration language build on top of python. It allows user 
to define options and the dependencies then reuse these options in their source 
code for customisable build.

Lunaix use LunaConfig to toggle various optional modules, subsystems or features
and allow parameterised kernel build.


## Design Motivation

LunaConfig is designed as an improvement over the old-school Kconfig, 
particularly to address its lack of hierarchical representation. Kconfig 
organises options into a large, flat list, even though they appear in a 
hierarchical menu structure. As a result, it can be very difficult to 
determine which menu a configuration option belongs to without diving into 
menuconfig.

## Basic Constructs

LunaConfig is presented as a file named `LConfig` and thus limited to one 
LConfig per directory. This is because LunaConfig enforce user to organise each 
directory to be a module or logical packaging of set of relavent functionalities.

Each LunaConfig files is comprised by these major constructs:

1. Config Component: `Terms` and `Groups`
2. Config Import
3. Native Python

We will describe them in details in the following sections

### Terms

> The object that hold the value for customisation

```py
def feature1() -> bool:
    return True
```

This defined an option named `feature1` with type of `bool` and the default
value of `True`.

### Groups

> The logical object that is used to organise the `terms` or other `groups`

A group is similar to term but without return type indication and return 
statement

And it is usually nested with other groups or terms.

```py
def group1():
    def feature1() -> bool:
        return True
```

### Import Mechanism

Multiple `LConfig`s may be defined across different sub-directory in large scale
project for better maintainability

LunaConfig allow you to import content of other LConfig using python's relative 
import feature:

```py
from . import module
```

This import mechanism works like `#include` directive in C preprocessor,
the `from . import` construct will be automatically intercepted by the LConfig 
interpreter and be replaced with the content from `./module/LConfig`

You can also address file in the deeper hierarchy of the directory tree, for 
example

```py
from .sub1.sub2 import module
```

This will be translated into `./sub1/sub2/module/LConfig`

It can also be used in any valid python conditional branches to support 
conditional import

```py
if condition_1:
    from . import module1
elif condition_2:
    from . import module2
```

### Native Python

Native Python code is fully supported in LunaConfig, this include everything 
like packages import, functions, and class definitions. LunaConfig has the 
ability to distinguish these code transparently from legitimate config code.

However, there is one exception for this ability. Since LunaConfig treat 
function definition as declaration of config component, to define a python 
native function you will need to decorate it with `@native`. For example:

```py
@native
def add(a, b):
    return a + b

def feature1() -> int:
    return add(1, 2)
```

If a native function is nested in a config component, it will not be affected 
by the scope and still avaliable globally. But this is not the case if it is 
nested by another native function.

If a config component is nested in a native function, then it is ignored by 
LunaConfig

## Term Typing

A config term require it's value type to be specified explicitly. The type can 
be a literal type, primitive type or composite type

### Literal Typing

A term can take a literal as it's type, doing this will ensure the value taken 
by the term to be exactly same as the given type

```py
# OK
def feature1() -> "value":
    return "value"
    # return "value2"

# Error
def feature1() -> "value":
    return "abc"
```

### Primitive Typing

A term can take any python's primitive type, the value taken by the term will 
be type checked rather than value checked

```py
# OK
def feature1() -> int:
    return 123

# Error
def feature1() -> int:
    return "abc"
```

### Composite Typing

Any literal type or primitive type can be composite together via some structure 
to form composite type. The checking on these type is depends on the composite 
structure used. 

#### Union Structure

A Union structure realised through binary disjunctive connector `|`. The term 
value must satisfy the type check against one of the composite type:

```py
def feature1() -> "a" | "b" | int:
    # value can be either:
    #  "a" or "b" or any integer
    return "a"
```

## Component Attributes

Each component have set of attributes to modify its behaviour and apperance, 
these attributes are conveyed through decorators

### Labels

> usage: `Groups` and `Terms`

Label provide a user friendly name for a component, which will be the first 
choice of the name displayed by the interactive configuration tool

```py
@"This is feature 1"
def feature1() -> bool:
    return True
```

### Readonly

> usage: `Terms`

Marking a term to be readonly prevent explicit value update, that is, manual 
update by user. Implicit value update initiated by `constrains` (more on this 
later) is still allowed. 

```py
@readonly
def feature1() -> bool:
    return True
```

### Visibility

> usage: `Groups` and `Terms`

A component can be marked to be hidden thus prevent it from displayed by the 
configuration tool, it does not affect the visibility in the code.

If the decorated target is a group, then it is inherited by all it's 
subordinates.

```py
@hidden
def feature1() -> bool:
    return True
```

### Flag

> usage: `Terms`

This is a short hand of both `@hidden` and `@readonly`

```py
@flag
def feature1() -> bool:
    return True
```

is same as

```py
@hidden
@readonly
def feature1() -> bool:
    return True
```

### Parent

> usage: `Groups` and `Terms`

Any component can be defined outside of the logical hierachial structure (i.e., 
the nested function) but still attached to it's physical hierachial structure.

```py
@parent := parent_group
def feature1() -> bool:
    return True

def parent_group():
    def feature2() -> bool:
        return False
```

This will assigned `feature1` to be a subordinate of `parent_group`. Note that 
the reference to `parent_group` does not required to be after the declaration.

It is equivalent to 

```py
def parent_group():
    def feature2() -> bool:
        return False
    
    def feature1() -> bool:
        return True
```

### Help Message

> usage: `Groups` and `Terms`

A help message will provide explainantion or comment of a component, to be used 
and displayed by the configuration tool.

The form of message is expressed using python's doc-string

```py
def feature1() -> bool:
    """
    This is help message for feature1
    """

    return True
```

## Directives

There are builtin directives that is used inside the component.

### Dependency

> usage: `Groups` and `Terms`

The dependency between components is described by various `require(...)` 
directives. 

This directive follows the python function call syntax and accept one argument 
of a boolean expression as depedency predicate.

Multiple `require` directives will be chainned together with logical conjunction 
(i.e., `and`)

```py
def feature1() -> bool:
    require (feature2 or feature3)
    require (feature5)
    
    return True
```

This composition is equivalent to `feature5 and (feature2 or feature3)`, 
indicate that the `feature1` require presences of both `feature5` and at least 
one of `feature2` or `feature3`.

If a dependency can not be satisfied, then the feature is disabled. This will 
cause it neither to be shown in configuration tool nor referencable in source 
code.

Note that the dependency check only perform on the enablement of the node but 
not visibility.

### Constrains (Inverse Dependency)

> usage: `Terms` with `bool` value type

The `when(...)` directive allow changing the default value based on the 
predicate evaluation on the value of other terms. Therefore, it can be only 
used by `Terms` with `bool` as value type.

Similar to `require`, it is based on the function call syntax and takes one 
argument of conjunctive expression (i.e., boolean expression with only `and` 
connectors).

Multiple `when` will be chained together using logical disjunction (i.e., `or`)

```py
def feature1() -> bool:
    when (feature2 is "value1" and feature5 is "value1")
    when (feature3)
```

Which means `feature1` will takes value of `True` if one of the following is 
satisfied:
 + both `feature2` and `feature5` has value of `"value1"`
 + `feature3` has value of True

Notice that we do not have `return` statement at the end as the precense of 
`when` will add a return automatically (or replace it if one exists)


## Validation

Since the language is based on Python, it's very tempting to pack in advanced 
features or messy logic into the config file, which can make it harder to 
follow—especially as the file grows in size.

LunaConfig recognises this issue and includes a built-in linter to help users 
identify such bad practices. It shows warnings by default but can be configured 
to raise errors instead.

Currently, LunaConfig detect the misuses based on these rules:

+ `dynamic-logic`: The presence of conditional branching that could lead to 
   complex logic. However, pattern matching is allowed.
+ `while-loop`, `for-loop`: The presence of any loop structure.
+ `class-def`: The presence of class definition
+ `complex-struct`: The present of complicated data structure such as `dict`.
+ `side-effect`: The presence of dynamic assignment of other config terms value.
+ `non-trivial-value`: The presence of non-trivial value being used as default 
   value. This include every things **other than**:
    + literal constant
    + template literal


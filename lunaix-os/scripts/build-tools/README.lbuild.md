# LunaBuild

LunaBuild is programmable source file selection tool. It does not build things
by itself, rather, it selects which source file should be feed as input to
other tranditional build tools such as make. The selection logic is completely
programmable and convey through `LBuild` using python syntax. 
Since the primary design goal for LunaBuild is simple, lightweight and
standalone. It introduce minimal customisations on the python syntax for better
readbility and it is essentially modified python.

## Functionalities

### Native Python Environment

LunaBuild is a superset of python, meaning that all existing functionalities of 
python language is still intacted and supported.

This will gives you maximum flexibility on defining your build logic.

### Import other `LBuild`

Multiple `LBuild`s may be defined across different sub-directory in large scale
project for better maintainability

LunaBuild allow you to import content of other LBuild using python's relative import
feature:

```py
from . import subdirectory
```

This import mechanism works like `#include` directive in C preprocessor,
the `from . import` construct will automatically intercepted by the LBuild interpreter and 
be replaced with the content from `./subdirectory/LBuild`

You can also address file in the deeper hierarchy of the directory tree, for example

```py
from .sub1.sub2 import sub3
```

This will be translated into `./sub1/sub2/sub3/LBuild`

It can also be used in any valid python conditional branches to support 
conditional import

```py
if feature1_enabled:
    from . import feature1
elif feature3_enabled
    from . import feature3
```


### Scoped Data Banks

Almost every build systems is evolved around the list data structure, as its
main task is to collect all interested source files, headers or other build 
process relative information. This is also true for LunaBuild.

To better represent this in a more readable way, LunaBuild introduce the concept 
of scoped data banks, represented as a automatic global object in the script.

Take a look in the example:

```py
src.c += "source1.c", "source2.c"
```

This will append `"source1.c"` and `"source2.c"` into the list named `c` under
the scope of `src`. Which can be clearly interpreted as "collection of c source files"

It is also important to notice that not all databanks are in the forms of list.
Some data banks served special purpose, as we will see below.

LunaBuild defines the following databanks and scope:

+ `src` (source files):
    + `c` (c files, type: `[]`)
    + `h` (headers or include directories, type: `[]`)
+ `flag` (source files):
    + `cc` (compiler flags, type: `[]`)
    + `ld` (linker flags, type: `[]`)
+ `config` (configuration options from `LConfig`)
    + `CONFIG_<OptionName>` (any valid config name, type: `Any`)
+ `env` (environmental variables)
    + `<Name>` (any valid env variable name, type: `Any`)

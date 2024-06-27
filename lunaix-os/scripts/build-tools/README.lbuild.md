# LunaBuild

LunaBuild is programmable source file selection tool. It does not build things
by itself, rather, it selects which source file should be feed as input to
other tranditional build tools such as make. The selection logic is completely
programmable and convey through `LBuild` file which is essentially a python
script. As the primary design goal for LunaBuild is simple, lightweight and
standalone. It just plain python, with some extra predefined functions and
automatic variables, so does not force user to learn some other weird domain
specific language (yes, that's you, CMake!).

## Usage

Invoke `./luna_build.py <root_lbuild> -o <out_dir>`. It will output two file
to the `<out_dir>`: `sources.list` and `headers.list`. Contains all the source
files and header files to be used by the build process.

## Core Functions

LunaBuild provide following function to help user select source files.

### [func] `use(lbuild_path)`

Include another LBuild file. `lbuild_path` is the path relative to current
directory, pointed to the file. It can be also pointed to a directory, for
which the LBuild file is inferred as `$lbuild_path/LBuild`.

For example:

```py
use("dir")
use("dir/LBuild")
```

both are equivalent.

### [func] `sources(src_list)`

Select a list of source files, all paths used are relative to current
directory. For example,

```py
sources([ "a.c", "b.c", "c.c" ])
```

### [func] `headers(src_list)`

Select a list of header file or include directory, all paths used are
relative to current directory. For example,

```py
headers([ "includes/", "includes/some.h" ])
```

### [func] `configured(name)`

Check whether a configuration is present, the name for the configuration
is defined by external configuration provider.

### [func] `config(name)`

Read the value of a configuration, raise exception is not exists.

### [var] `_script`

Automatic variable, a path to the current build file.

## Short-hands

LunaBuild provide some useful short-hand notations

### Conditional Select

Suppose you have two source files `src_a.c` and `src_b.c`, for which
their selection will be depends on the value of a configuration
`WHICH_SRC`. A common way is to use `if-else` construct

```py
if config("WHICH_SRC") == "select_a":
    sources("src_a.c")
elif config("WHICH_SRC") == "select_b":
    sources("src_b.c")
```

LunaBuild allow you to short hand it as such:

```py
sources({
    config("WHICH_SRC"): {
        "select_a": "src_a.c",
        "select_b": "src_b.c",
        # more...
    }
})
```

It can also be extended easily for multiple choices and allow nesting.

You may also notice we no longer wrap the parameter with square bracket,
this is also another short-hand, the array notation is not needed when
there is only one element to add.

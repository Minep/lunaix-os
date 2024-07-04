from lbuild.api import BuildGenerator
from lbuild.common import BuildEnvironment
from lib.utils import join_path
from os import getenv

class MakefileBuildGen(BuildGenerator):
    def __init__(self, out_dir, name = "lbuild.mkinc") -> None:
        self.__path = join_path(out_dir, name)
    
    def emit_makearray(self, name, values):
        r = []
        r.append(f"define {name}")
        for v in values:
            r.append(v)
        r.append("endef")
        return r

    def generate(self, env: BuildEnvironment):
        path = env.to_wspath(self.__path)
        lines = []

        opts = env.get_object("CC_OPTS", [])
        lines.append("CFLAGS += %s"%(" ".join(opts)))
        
        opts = env.get_object("LD_OPTS", [])
        lines.append("LDFLAGS += %s"%(" ".join(opts)))

        arr = self.emit_makearray("_LBUILD_SRCS", env.srcs())
        lines += arr

        arr = self.emit_makearray("_LBUILD_HDRS", env.headers())
        lines += arr

        arr = self.emit_makearray("_LBUILD_INCS", env.includes())
        lines += arr

        with open(path, 'w') as f:
            f.write("\n".join(lines))
        

def install_lbuild_functions(_env: BuildEnvironment):
    def set_opts(env: BuildEnvironment, name, opts, override):
        if not isinstance(opts, list):
            opts = [opts]
        
        _opts = env.get_object(name, [])

        if override:
            _opts = opts
        else:
            _opts += opts

        env.set_object(name, _opts)

    def compile_opts(env: BuildEnvironment, opts, override=False):
        set_opts(env, "CC_OPTS", opts, override)

    def linking_opts(env: BuildEnvironment, opts, override=False):
        set_opts(env, "LD_OPTS", opts, override)

    def env(env, name, default=None):
        return getenv(name, default)


    _env.add_external_func(compile_opts)
    _env.add_external_func(linking_opts)
    _env.add_external_func(env)
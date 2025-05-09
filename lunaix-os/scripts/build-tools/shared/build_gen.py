from os.path      import isdir, exists
from lbuild.build import BuildEnvironment
from lbuild.scope import ScopeAccessor

def gen_source_file(subscope : ScopeAccessor):
    items = " ".join(subscope.values())
    return [f"BUILD_SRCS := {items}"]

def gen_header_file(subscope : ScopeAccessor):
    inc, hdr = [], []
    for x in subscope.values():
        if not exists(x):
            print(f"warning: '{x}' does not exists, skipped")
            continue

        if isdir(x):
            inc.append(x)
        else:
            hdr.append(x)
            
    return [
        f"BUILD_INC := {' '.join(inc)}",
        f"BUILD_HDR := {' '.join(hdr)}",
    ]

def gen_ccflags(subscope : ScopeAccessor):
    items = " ".join(subscope.values())
    return [f"BUILD_CFLAGS := {items}"]

def gen_ldflags(subscope : ScopeAccessor):
    items = " ".join(subscope.values())
    return [f"BUILD_LDFLAGS := {items}"]


class BuildScriptGenerator:
    def __init__(self, env: BuildEnvironment):
        self.__gen_policy = {
            "src": {
                "c": gen_source_file,
                "h": gen_header_file
            },
            "flag": {
                "cc": gen_ccflags,
                "ld": gen_ldflags
            }
        }

        self.__env = env

    def __gen_lines(self, scope, subscope):
        
        policy = self.__gen_policy.get(scope.name)
        if policy is None:
            print( "warning: no associated policy with"
                  f"'{scope.name}'"
                   ", skipped")
            return []
        
        policy = policy.get(subscope.name)
        if policy is None:
            print( "warning: no associated policy with "
                  f"'{scope.name}.{subscope.name}' "
                   ", skipped")
            return []
        
        return policy(subscope)
    
    def generate(self, file):
        lines = []

        for scope in self.__env.scopes.values():
            for subscope in scope.accessors():
                lines += self.__gen_lines(scope, subscope)

        with open(file, 'w') as f:
            f.write('\n'.join(lines))
        
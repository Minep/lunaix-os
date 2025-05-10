import ast
import textwrap

from lib.utils  import ConfigAST, ConfigASTVisitor
from .common     import NodeProperty, ConfigNodeError, ValueTypeConstrain
from .nodes      import GroupNode, TermNode
from .sanitiser  import TreeSanitiser

class NodeBuilder(ConfigASTVisitor):
    def __init__(self, env):
        super().__init__()

        self.__env = env
        self.__level = []
        self.__noncfg_astns = []

    def __pop_and_merge(self):
        if len(self.__level) == 0:
            return
        
        if len(self.__level) == 1:
            self.__level.pop()
            return
        
        node = self.__level.pop()
        prev = self.__level[-1]

        assert isinstance(prev, GroupNode)

        prev.add_child(node)

    def __push(self, cfg_node):
        self.__level.append(cfg_node)

    def __check_literal_expr(self, node):
        return (
            isinstance(node, ast.Expr) 
            and isinstance(node.value, ast.Constant) 
            and isinstance(node.value.value, str)
        )

    def _visit_fndef(self, node):
        if hasattr(node, "__builtin"):
            self.__noncfg_astns.append(node)
            return
        
        cfgn_type = TermNode
        if not node.returns:
            cfgn_type = GroupNode

        cfgn = cfgn_type(self.__env, self.current_file(), node.name)
        
        self.__push(cfgn)

        try:
            for decor in node.decorator_list:
                cfgn.apply_decorator(decor)
            
            astns = []
            help_text = ""
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef):
                    self._visit_fndef(sub)
                    continue

                if self.__check_literal_expr(sub):
                    help_text += sub.value.value
                    continue

                astns.append(sub)

            NodeProperty.Token[cfgn] = node
            NodeProperty.HelpText[cfgn] = textwrap.dedent(help_text)
            
            if cfgn_type is TermNode:
                NodeProperty.Type[cfgn] = ValueTypeConstrain(cfgn, node.returns)
            
            astns.append(ast.Pass())
            cfgn.set_node_body(astns)
            
            self.__env.register_node(cfgn)
        
        except ConfigNodeError as e:
            raise e
        except Exception as e:
            msg = e.args[0] if len(e.args) > 0 else type(e).__name__
            raise cfgn.config_error(msg)

        self.__pop_and_merge()

    def visit(self, node):
        super().visit(node)

        if isinstance(node, ast.Module):
            return

        if not isinstance(node, ast.FunctionDef):
            self.__noncfg_astns.append(node)

    @staticmethod
    def build(env, rootfile):
        build = NodeBuilder(env)
        ast = ConfigAST(rootfile)
        
        env.reset()
        ast.visit(TreeSanitiser())
        ast.visit(build)

        for node in env.nodes():
            node.apply_node_body()

        env.set_exec_context(build.__noncfg_astns)
        env.relocate_children()
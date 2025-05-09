import readline, textwrap

from rlcompleter    import Completer
from lcfg2.config   import ConfigEnvironment
from .common        import ShconfigException, get_config_name
from .commands      import Commands

class ConfigNameCompleter(Completer):
    def __init__(self, env: ConfigEnvironment):
        super().__init__(None)

        self.__options = []
        self.__config_set= [
            get_config_name(x._name)
            for x in env.terms()
        ]

    def complete(self, text, state):
        if state == 0:
            text = text if text else ""
            self.__options = [
                x for x in self.__config_set if x.startswith(text)]
        
        return None if not self.__options else self.__options[state]


def next_input(cmds: Commands):
    line = input("shconfig> ")

    if len(line) == 0:
        return True
    
    parts = line.split(' ')
    name, args = parts[0], parts[1:]

    if name in ['q', 'exit']:
        return False
    
    if name == "q!":
        raise KeyboardInterrupt()
    
    if name.startswith("CONFIG_"):
        cmds.call("opt", name)
        return True

    cmds.call(name, *args)
    return True

def shconfig(env: ConfigEnvironment):
    print(
        "\n",
        textwrap.dedent(
            
            """
            Lunaix Interactive Configurator (shconfig)
            
            Type "help" to see all commands avaliables
            Type "q" or "exit" to confirm and exit
            Type "q!" or use ^C to discard and abort

            """
        ).strip(), "\n")

    cmds = Commands(env)
    cmpleter = ConfigNameCompleter(env)
    readline.parse_and_bind('tab: complete')
    readline.set_completer(cmpleter.complete)

    while True:
        try:
            if not next_input(cmds):
                return True
        except ShconfigException as e:
            print(str(e))
            continue
        except KeyboardInterrupt as e:
            return False
        except Exception as e:
            raise e
            return False

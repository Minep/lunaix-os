from .api import TypeProviderBase
from .utils import is_primitive, is_basic

class PrimitiveType(TypeProviderBase):
    def __init__(self, type) -> None:
        super().__init__(type)

    @staticmethod
    def typedef_matched(typedef):
        return is_primitive(typedef)
    
    def check(self, val):
        return isinstance(val, self._type)
    
    def serialise(self, val):
        return str(val)
    
    def deserialise(self, val):
        if val.lower() == "true":
            return True
        elif val.lower() == "false":
            return False
        
        return self._type(val)
    
    def parse_input(self, input_str):
        return self.deserialise(input_str)
    
    def to_input(self, val):
        return self.serialise(val)
    
    def __str__(self) -> str:
        if isinstance(self._type, type):
            return f"any with type of {self._type}"
        return f"exact of value '{self._type}'"


class MultipleChoiceType(PrimitiveType):
    def __init__(self, type) -> None:
        super().__init__(type)

    @staticmethod
    def typedef_matched(typedef):
        if not isinstance(typedef, list):
            return False
        return all([is_basic(x) for x in typedef])

    def check(self, val):
        if not is_basic(val):
            return False
        return val in self._type
    
    def parse_input(self, input_str):
        return super().parse_input(input_str)

    def deserialise(self, val):
        if val.lower() == "true":
            return True
        elif val.lower() == "false":
            return False
        
        for sv in self._type:
            if val != str(sv):
                continue
            return type(sv)(val)

        return None
    
    def allow_none(self):
        return None in self._type
    
    def __str__(self) -> str:
        accepted = [f"  {t}" for t in self._type]
        return "\n".join([
            "choose one: \n",
            *accepted
        ])

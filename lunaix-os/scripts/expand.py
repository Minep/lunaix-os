import jinja2
import re
import argparse
import math
import json
from pathlib import Path
from abc import ABC, abstractmethod

class Preprocessor:
    reHex = re.compile(r"^0x([0-9a-fA-F]+)$")
    reGranuel = re.compile(r"^(?P<num>[0-9]+)@(?P<g>.+)$")
    reMacroRef = re.compile(r"^\*(?P<var>[a-zA-z0-9]+)$")
    reInt = re.compile(r"^[0-9]+$")
    def __init__(self) -> None:
        pass

    @staticmethod
    def expand_str(s: str, param_dict):
        if Preprocessor.reInt.match(s) is not None:
            return int(s)
        
        mo = Preprocessor.reHex.match(s)
        if mo is not None:
            return int(s, 16)
        
        mo = Preprocessor.reGranuel.match(s)
        if mo is not None:
            mg = mo.groupdict()
            num = int(mg['num'])
            granuel = param_dict["granule"][mg['g']]
            return num * granuel
        
        mo = Preprocessor.reMacroRef.match(s)
        if mo is not None:
            mg = mo.groupdict()
            return param_dict[mg['var']]

        return s.format_map(param_dict)

class DataObject(ABC):
    def __init__(self, name, record):
        self.key = name
        self._record = record
        self.user_field = {}
        self.ctrl_field = {}
        self._parse(record)

    @staticmethod
    def create(record):
        return DataObject.create("", record)

    @staticmethod
    def create(key, record):
        if PrimitiveType.can_create(record):
            return PrimitiveType(record)
        
        name = key
        t = name if "$type" not in record else record['$type']
        
        if "$name" in record:
            name = record["$name"].strip()

        if not key.startswith('@') and "$type" not in record:
            return PlainOldObject(name, record)
        
        t = t.strip("@")
        if t == "list":
            return RangedObject(name, record)
        elif t == "foreach":
            return ForEachIndexObject(name, record)
        elif t == "case_range_index":
            return Condition(record)
        elif t == "data":
            return PlainOldObject(name, record)
        elif t == "define":
            return VariableDeclarationObject(record)
        elif t == "memory_map":
            return MemoryMapObject(record)
        else:
            return RawObject(name, record)

    def _parse(self, record):
        for k, v in record.items():
            if k.startswith("$"):
                self.ctrl_field[k.strip("$")] = FieldType.create(k, v)
            elif k.startswith("@"):
                self.ctrl_field[k.strip("@")] = DataObject.create(k, v)
            else:
                self.user_field[k] = DataObject.create(k, v)

    def expand(self, param={}):
        obj2 = {}
        for f in self.ctrl_field.values():
            if not isinstance(f, DataObject):
                continue
            obj2.update(**f.expand(param))

        obj = {}
        _param = {**param, **obj2}
        for k, v in self.user_field.items():
            if isinstance(v, DataObject):
                obj[k] = v.expand(_param)
            else:
                obj[k] = v

        return {**obj, **obj2}
    

class FieldType:
    def __init__(self, record) -> None:
        self._record = record
        self._parse(record)

    @staticmethod
    def create(field, value):
        if field == "$range":
            return RangeType(value)
        else:
            return value
    
    @abstractmethod
    def _parse(self, record):
        pass

    @abstractmethod
    def get(self, param):
        pass

    def getraw(self):
        return self.__record

class PrimitiveType(DataObject):
    def __init__(self, record) -> None:
        super().__init__("", record)

    @staticmethod
    def can_create(value):
        return type(value) in (str, int, bool)

    def _parse(self, record):
        if type(record) not in (str, int, bool):
            raise Exception(f"{type(self).__name__} require primitive type input")
        self.val = record

        if type(record) == str:
            self.__get_fn = self.__process_str
        else:
            self.__get_fn = lambda x: self.val

    def __process_str(self, param):
        return Preprocessor.expand_str(self.val, param)
    
    def expand(self, param={}):
        return self.__get_fn(param)

class RangeType(FieldType):
    def __init__(self, record) -> None:
        self.__ranged_component = re.compile(r"^(?P<index>[^.]+)$|^(?P<start>.+?)\.\.(?P<end>.+)$")
        super().__init__(record)

    def _parse(self, record):
        return super()._parse(record)
    
    def get(self, param):
        record = self._record.strip('[]')
        
        self.__value=[]
        for component in record.split(','):
            component = component.strip()
            mo = self.__ranged_component.match(component)
            if mo is None:
                raise Exception(f"value '{component}' is not valid range component")
            
            mo = mo.groupdict()
            if mo["index"] is not None:
                self.__value.append(Preprocessor.expand_str(mo['index'], param))
            else:
                start = Preprocessor.expand_str(mo['start'], param)
                end = Preprocessor.expand_str(mo['end'], param)
                self.__value += [x for x in range(start, end + 1)]
        return self.__value

    def getraw(self):
        return self._record
    
class VariableDeclarationObject(DataObject):
    def __init__(self, record):
        super().__init__("", record)
    
    def _parse(self, record):
        return super()._parse(record)
    
    def expand(self, param={}):
        obj = super().expand(param)
        param.update(**obj)
        return {}

class Condition(DataObject):
    def __init__(self, record):
        super().__init__("", record)
    
    def _parse(self, record):
        super()._parse(record)
        if "range" not in self.ctrl_field:
            raise Exception("condition body must contains valid range case")
        if "true" not in self.ctrl_field:
            raise Exception("condition body must contains 'True' handling case")
        
    
    def expand(self, param={}):
        self.__range_lst = self.ctrl_field["range"].get(param)
        if param["index"] in self.__range_lst:
            return self.ctrl_field["true"].expand(param)
        elif "else" in self.ctrl_field:
            return self.ctrl_field["else"].expand(param)
        return {}
    
class ArrayObject(DataObject):
    def __init__(self, record, 
                 nested_array = False, 
                 el_factory = lambda x: DataObject.create("", x)):
        self._el_factory = el_factory
        self._nested_array = nested_array

        super().__init__("", record)
    
    def _parse(self, record):
        if not isinstance(record, list):
            raise Exception(f"{type(self).__name__} require array input")
        
        self.content = []
        for x in record:
            self.content.append(self._el_factory(x))
    
    def expand(self, param={}):
        result = []
        for x in self.content:
            obj = x.expand(param)
            if isinstance(obj, list) and not self._nested_array:
                result += [*obj]
            else:
                result.append(obj)
        
        return result
    
class MemoryMapObject(DataObject):
    class GranuleObject(DataObject):
        def __init__(self, record):
            super().__init__("", record)
            
        def _parse(self, record):
            self.__granules = {}
            for k, v in record.items():
                self.__granules[k] = DataObject.create(k, v)

        def expand(self, param={}):
            granules = {}
            for k, v in self.__granules.items():
                val = v.expand(param)

                if not isinstance(val, int):
                    raise Exception("The granule definition must be either integer or int-castable string")
                
                granules[k] = val
                
            return {**granules}
        
    def __init__(self, record):
        super().__init__("", record)

    def _parse(self, record):
        for k, v in record.items():
            if k.startswith("$"):
                self.ctrl_field[k.strip("$")] = FieldType.create(k, v)
            elif k.startswith("@"):
                self.ctrl_field[k.strip("@")] = DataObject.create(k, v)
        
        if "granule" in record:
            self.__g = MemoryMapObject.GranuleObject(record["granule"])

        if "regions" in record:
            self.__regions = ArrayObject(record["regions"])

        if "width" in record:
            self.__width = DataObject.create("width", record["width"])

    def __process(self, start_addr, idx, regions, size_lookahead = False):
        if idx >= len(regions):
            raise Exception("Unbounded region definition")
        
        e = regions[idx]

        if "block" in e:
            b = e["block"] - 1
            start_addr = (start_addr + b) & ~b

        if "start" not in e:
            e["start"] = start_addr
        elif e["start"] < start_addr:
            raise Exception(f"starting addr {hex(e['start'])} overrlapping with {hex(start_addr)}")
        else:
            start_addr = e["start"]
        
        if "size" not in e:
            if size_lookahead:
                raise Exception("could not infer size from unbounded region")
            tmp_addr = self.__process(start_addr, idx + 1, regions, size_lookahead=True)
            e["size"] = tmp_addr - start_addr
        
        if not size_lookahead:
            start_addr += e["size"]
        
        return start_addr
    
    def expand(self, param={}):
        super().expand(param)

        g = self.__g.expand(param)

        param["granule"] = g

        width = self.__width.expand(param)
        if not isinstance(width, int):
            raise Exception("'width' attribute must be integer")

        regions = self.__regions.expand(param)
        
        start_addr = 0
        for i in range(len(regions)):
            start_addr = self.__process(start_addr, i, regions)
        
        if math.log2(start_addr) > width:
            raise Exception("memory size larger than speicified address width")

        return {
            "granule": g,
            "regions": regions
        }

class ForEachIndexObject(DataObject):
    def __init__(self, name, record):
        super().__init__(name, record)
        self.steps = []
        for k, v in record.items():
            self.steps.append(DataObject.create(k, v))

    def _parse(self, record):
        super()._parse(record)
    
    def expand(self, param={}):
        if "index" not in param:
            raise Exception(f"'{type(self).__name__}' require parameter 'index'.")
        obj = {}
        for cond in self.steps:
            obj.update(**cond.expand(param))
        return obj
    
class RawObject(DataObject):
    def __init__(self, name, record):
        super().__init__(name, record)
    
    def _parse(self, record):
        return super()._parse(record)

    def expand(self, param={}):
        return super().expand(param)

class PlainOldObject(DataObject):
    def __init__(self, name, record):
        super().__init__(name, record)

    def _parse(self, record):
        return super()._parse(record)

    def expand(self, param={}):
        return super().expand(param)

class RangedObject(DataObject):
    def __init__(self, name, record):
        super().__init__(name, record)
    
    def _parse(self, record):
        super()._parse(record)

    def expand(self, param={}):
        if "range" not in self.ctrl_field:
            raise Exception("RangedObject with ranged type must have 'range' field defined")
        
        out_lst = []
        indices = self.ctrl_field["range"].get(param)
        for i in indices:
            param["index"] = i
            out_lst.append(super().expand(param))
        
        return out_lst
    
def aligned(v, a):
    return v & ~(a - 1)
    
class TemplateExpander:
    def __init__(self, template_path, project_path, arch) -> None:
        self.arch = arch
        self.tbase_path = template_path.joinpath(arch)
        self.pbase_path = project_path

        self.__helper_fns = {
            "align": aligned,
            "hex": lambda x: hex(x)
        }

        self.__load_config()
        self.__load_mappings()

    def __load_config(self):
        self.data = {}
        cfg_file: Path = self.tbase_path.joinpath("config.json")
        if not cfg_file.is_file():
            raise Exception(f"config not found. ({cfg_file})")
        
        obj = json.loads(cfg_file.read_text())
        for k, v in obj.items():
            o = DataObject.create(k, v).expand()
            self.data[k] = o

    def __load_mappings(self):
        self.mapping = {}
        mapping_file: Path = self.tbase_path.joinpath("mappings")
        if not mapping_file.is_file():
            raise Exception(f"config not found. ({mapping_file})")
        
        with mapping_file.open() as f:
            for l in f:
                l = l.strip()

                if not l:
                    continue

                src, dest = l.split("->")
                src = src.strip()

                if src in self.mapping:
                    raise Exception(f"repeating entry ({src})")
                
                self.mapping[src] = dest.strip()

    def render(self):
        for k, v in self.mapping.items():
            src: Path = self.tbase_path.joinpath(k)
            dest: Path = self.pbase_path.joinpath(v)
            if not src.is_file():
                continue

            if not dest.parent.exists():
                dest.parent.mkdir(parents=True)
            
            self.data["template"] = k
            template = jinja2.Template(src.read_text(), trim_blocks=True)
            template.globals.update(**self.__helper_fns)
            out = template.render(data=self.data)

            dest.write_text(out)

            print(f"rendered: {k} -> {v}")

import pprint

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--arch", default='i386')
    parser.add_argument("-twd", "--template_dir", default=str(Path.cwd()))
    parser.add_argument("-pwd", "--project_dir", default=str(Path.cwd()))

    args = parser.parse_args()

    expander = TemplateExpander(Path(args.template_dir), Path(args.project_dir), args.arch)
    
    expander.render()
    # pprint.pprint(expander.data)

if __name__ == "__main__":
    main()
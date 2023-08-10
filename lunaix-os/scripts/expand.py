import jinja2
import re
import argparse
import sys
import json
from pathlib import Path
from abc import ABC, abstractmethod

class ControlAction(ABC):
    def __init__(self, record) -> None:
        self.__record = record
        self._parse(record)

    @staticmethod
    def create(field, value):
        if field == "$range":
            return RangeAction(value)
        else:
            return value
    
    @abstractmethod
    def _parse(self, record):
        pass

    @abstractmethod
    def action(self, data, param):
        pass

    def get(self):
        return self.__record

class RangeAction(ControlAction):
    def __init__(self, record) -> None:
        self.__ranged_component = re.compile(r"^(?P<index>[0-9]+)$|^(?P<start>[0-9]+)..(?P<end>[0-9]+)$")
        super().__init__(record)

    def _parse(self, record):
        if not (record.startswith('[') and record.endswith(']')):
            raise Exception(f"'{record}' is not valid range expression")
        record = record.strip('[]')
        
        self.__value=[]
        for component in record.split(','):
            component = component.strip()
            mo = self.__ranged_component.match(component)
            if mo is None:
                raise Exception(f"value '{component}' is not valid range component")
            
            mo = mo.groupdict()
            if mo["index"] is not None:
                self.__value.append(int(mo['index']))
            else:
                start = int(mo['start'])
                end = int(mo['end'])
                self.__value += [x for x in range(start, end + 1)]

        self.__value = sorted(self.__value)

    def action(self, data, param):
        return super().action(data, param)

    def get(self):
        return self.__value
    

class DataObject(ABC):
    def __init__(self, name, record):
        self.key = name
        self.user_field = {}
        self.ctrl_field = {}
        self._parse(record)

    @staticmethod
    def create(record):
        return DataObject.create("", record)

    @staticmethod
    def create(key, record):
        if not isinstance(record, dict):
            return record
        
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
        else:
            return RawObject(name, record)

    @abstractmethod
    def _parse(self, record):
        for k, v in record.items():
            if k.startswith("$"):
                self.ctrl_field[k.strip("$")] = ControlAction.create(k, v)
            elif k.startswith("@"):
                self.ctrl_field[k.strip("@")] = DataObject.create(k, v)
            else:
                self.user_field[k] = DataObject.create(k, v)

    @abstractmethod
    def expand(self, param={}):
        obj = { **self.user_field }
        for f in self.ctrl_field.values():
            if not isinstance(f, DataObject):
                continue
            obj.update(**f.expand(param))
        return obj

class Condition(DataObject):
    def __init__(self, record):
        super().__init__("", record)
    
    def _parse(self, record):
        super()._parse(record)
        if "range" not in self.ctrl_field:
            raise Exception("condition body must contains valid range case")
        if "true" not in self.ctrl_field:
            raise Exception("condition body must contains 'True' handling case")
        
        self.__range_lst = self.ctrl_field["range"].get()
    
    def expand(self, param={}):
        if param["index"] in self.__range_lst:
            return self.ctrl_field["true"].expand(param)
        elif "else" in self.ctrl_field:
            return self.ctrl_field["else"].expand(param)
        return {}

class ForEachIndexObject(DataObject):
    def __init__(self, name, record):
        super().__init__(name, record)
        self.conditions = []
        for k, v in record.items():
            self.conditions.append(DataObject.create(k, v))

    def _parse(self, record):
        super()._parse(record)
    
    def expand(self, param={}):
        if "index" not in param:
            raise Exception(f"'{type(self).__name__}' require parameter 'index'.")
        obj = {}
        for cond in self.conditions:
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
        return {
            self.key: super().expand(param)
        }

class RangedObject(DataObject):
    def __init__(self, name, record):
        super().__init__(name, record)
    
    def _parse(self, record):
        super()._parse(record)

    def expand(self, param={}):
        if "range" not in self.ctrl_field:
            raise Exception("RangedObject with ranged type must have 'range' field defined")
        
        out_lst = []
        indices = self.ctrl_field["range"].get()
        for i in indices:
            param["index"] = i
            self.user_field["index"] = i
            out_lst.append(super().expand(param))
        
        return {
            self.key: out_lst
        }
    
class TemplateExpander:
    def __init__(self, template_path, project_path, arch) -> None:
        self.arch = arch
        self.tbase_path = template_path.joinpath(arch)
        self.pbase_path = project_path

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
            self.data.update(**o)

    def __load_mappings(self):
        self.mapping = {}
        mapping_file: Path = self.tbase_path.joinpath("mappings")
        if not mapping_file.is_file():
            raise Exception(f"config not found. ({mapping_file})")
        
        with mapping_file.open() as f:
            for l in f:
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
            
            template = jinja2.Template(src.read_text(), trim_blocks=True)
            out = template.render(data=self.data)

            dest.write_text(out)

            print(f"rendered: {k} -> {v}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--arch", default='i386')
    parser.add_argument("-twd", "--template_dir", default=str(Path.cwd()))
    parser.add_argument("-pwd", "--project_dir", default=str(Path.cwd()))

    args = parser.parse_args()

    expander = TemplateExpander(Path(args.template_dir), Path(args.project_dir), args.arch)
    expander.render()

if __name__ == "__main__":
    main()
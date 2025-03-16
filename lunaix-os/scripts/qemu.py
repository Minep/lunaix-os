#!/usr/bin/env python 

import subprocess, time, os, re, argparse, json
from pathlib import PurePosixPath
import logging
import uuid

logger = logging.getLogger("auto_qemu")
logging.basicConfig(level=logging.INFO)

g_lookup = {}

subsitution = re.compile(r"\$[A-Za-z_][A-Za-z0-9_]*")

def do_expand(val, path):
    global g_lookup

    parts = []
    prev_pos = 0
    iters = subsitution.finditer(val)
    for _, m in enumerate(iters, start=1):
        start_pos = m.start()
        end_pos = m.end()

        parts.append(val[prev_pos:start_pos])

        key = val[start_pos + 1:end_pos]
        if key not in g_lookup:
            raise Exception(
                f"reference key {key} (config: {path}) is not defined")

        parts.append(g_lookup[key])
        prev_pos = end_pos

    parts.append(val[prev_pos:])
    return "".join(parts)

def get_config(opt, path, default=None, required=False):
    _path = PurePosixPath(path)
    for p in _path.parts:
        if p in opt:
            opt = opt[p]
            continue
        if required:
            raise Exception(f"config: {path} is required")
        return default
        
    if not isinstance(opt, str):
        return opt

    return do_expand(opt, path)

def join_attrs(attrs):
    return ",".join(attrs)

def get_uniq():
    return uuid.uuid4().hex[:8]

def map_bool(b):
    return "on" if b else "off"
    


#################################
# IO Backend Definitions  
#

class IOBackend:
    def __init__(self, opt, id_prefix="io") -> None:
        self._type = get_config(opt, "type", required=True)
        self._logfile = get_config(opt, "logfile")
        self._id = f"{id_prefix}.{get_uniq()}"

    def get_options(self):
        opts = []
        if self._logfile:
            opts.append(f"logfile={self._logfile}")
        return opts
    
    def to_cmdline(self):
        return join_attrs([
            self._type, f"id={self._id}", *self.get_options()
        ])
    
    def name(self):
        return self._id

class FileIOBackend(IOBackend):
    def __init__(self, opt) -> None:
        super().__init__(opt)
        self.__path = get_config(opt, "path", required=True)

    def get_options(self):
        opts = [
            f"path={self.__path}"
        ]
        return opts + super().get_options()

class SocketIOBackend(IOBackend):
    def __init__(self, opt) -> None:
        super().__init__(opt)
        self.__protocol = self._type
        self._type = "socket"
        self.__host = get_config(opt, "host", default="localhost")
        self.__port = get_config(opt, "port", required=True)
        self.__server = bool(get_config(opt, "server", True))
        self.__wait = bool(get_config(opt, "wait", True))

    def get_options(self):
        opts = [
            f"host={self.__host}",
            f"port={self.__port}",
            f"server={map_bool(self.__server)}",
            f"wait={map_bool(self.__wait)}",
        ]
        if self.__protocol == "telnet":
            opts.append("telnet=on")
        if self.__protocol == "ws":
            opts.append("websocket=on")
        return opts + super().get_options()
    
def select_backend(opt):
    bopt = get_config(opt, "io", required=True)
    backend_type = get_config(bopt, "type", required=True)
    
    if backend_type in ["telnet", "ws", "tcp"]:
        return SocketIOBackend(bopt)
    
    if backend_type in ["file", "pipe", "serial", "parallel"]:
        return FileIOBackend(bopt)
    
    return IOBackend(bopt)



#################################
# QEMU Emulated Device Definitions
#

class QEMUPeripherals:
    def __init__(self, name, opt) -> None:
        self.name = name
        self._opt = opt

    def get_qemu_opts(self) -> list:
        pass

class ISASerialDevice(QEMUPeripherals):
    def __init__(self, opt) -> None:
        super().__init__("isa-serial", opt)

    def get_qemu_opts(self):
        chardev = select_backend(self._opt)

        cmds = [ 
            "isa-serial", 
           f"id=com.{get_uniq()}", 
           f"chardev={chardev.name()}" 
        ]

        return [ 
            "-chardev", chardev.to_cmdline(),
            "-device", join_attrs(cmds)
         ]
    
class PCISerialDevice(QEMUPeripherals):
    def __init__(self, opt) -> None:
        super().__init__("pci-serial", opt)

    def get_qemu_opts(self):
        chardev = select_backend(self._opt)

        cmds = [ 
            "pci-serial", 
           f"id=uart.{get_uniq()}", 
           f"chardev={chardev.name()}" 
        ]

        return [ 
            "-chardev", chardev.to_cmdline(),
            "-device", join_attrs(cmds)
         ]
    
class AHCIBus(QEMUPeripherals):
    def __init__(self, opt) -> None:
        super().__init__("ahci", opt)
        
    def __create_disklet(self, index, bus, opt):
        d_type = get_config(opt, "type",   default="ide-hd")
        d_img  = get_config(opt, "img",    required=True)
        d_ro   = get_config(opt, "ro",     default=False)
        d_fmt  = get_config(opt, "format", default="raw")
        d_id   = f"disk_{index}"

        if not os.path.exists(d_img):
            logger.warning(f"AHCI bus: {d_img} not exists, skipped")
            return []
        
        return [
            "-drive", join_attrs([
                f"id={d_id}",
                f"file={d_img}",
                f"readonly={'on' if d_ro else 'off'}",
                f"if=none",
                f"format={d_fmt}"
            ]),
            "-device", join_attrs([
                d_type,
                f"drive={d_id}",
                f"bus={bus}.{index}"
            ])
        ]

    def get_qemu_opts(self):
        opt = self._opt
        name: str = get_config(opt, "name", required=True)
        name = name.strip().replace(" ", "_")
        cmds = [ "-device", f"ahci,id={name}" ]

        disklets = get_config(opt, "disks", default=[])
        for i, disk in enumerate(disklets):
            cmds += self.__create_disklet(i, name, disk)
        
        return cmds
    
class RTCDevice(QEMUPeripherals):
    def __init__(self, opt) -> None:
        super().__init__("rtc", opt)
    
    def get_qemu_opts(self):
        opt = self._opt
        base = get_config(opt, "base", default="utc")
        return [ "-rtc", f"base={base}" ]

class QEMUMonitor(QEMUPeripherals):
    def __init__(self, opt) -> None:
        super().__init__("monitor", opt)

    def get_qemu_opts(self):
        
        chardev = select_backend(self._opt)

        return [
            "-chardev", chardev.to_cmdline(),
            "-mon", join_attrs([
                chardev.name(),
                "mode=readline",
            ])
        ]

class QEMUDevices:
    __devs = {
        "isa-serial": ISASerialDevice,
        "ahci": AHCIBus,
        "rtc": RTCDevice,
        "hmp": QEMUMonitor,
        "pci-serial": PCISerialDevice
    }

    @staticmethod
    def get(name):
        if name not in QEMUDevices.__devs:
            raise Exception(f"device class: {name} is not defined")
        return QEMUDevices.__devs[name]



#################################
# QEMU Machine Definitions
#

class QEMUExec:
    

    def __init__(self, options) -> None:
        self._opt = options
        self._devices = []

        for dev in get_config(options, "devices", default=[]):
            dev_class = get_config(dev, "class")
            device = QEMUDevices.get(dev_class)
            self._devices.append(device(dev))
    
    def get_qemu_exec_name(self):
        pass

    def get_qemu_arch_opts(self):
        cmds = [
            "-machine", get_config(self._opt, "machine"),
            "-cpu", join_attrs([ 
                 get_config(self._opt, "cpu/type", required=True),
                *get_config(self._opt, "cpu/features", default=[]),
             ])
        ]

        return cmds

    def get_qemu_debug_opts(self):
        cmds = [ "-no-reboot", "-no-shutdown" ]
        debug = get_config(self._opt, "debug")
        if not debug:
            return cmds
        
        cmds.append("-S")
        cmds += [ "-gdb", f"tcp::{get_config(debug, 'gdb_port', default=1234)}" ]

        trace_opts = get_config(debug, "traced", [])
        for trace in trace_opts:
            cmds += [ "--trace", f"{trace}"]

        return cmds
    
    def get_qemu_general_opts(self):
        opts = [
            "-m", get_config(self._opt, "memory", required=True),
            "-smp", str(get_config(self._opt, "ncpu", default=1))
        ]

        kopts = get_config(self._opt, "kernel")
        if kopts:
            opts += [
                "-kernel", get_config(kopts, "bin", required=True),
                "-append", get_config(kopts, "cmd", required=True)
            ]

            dtb = get_config(kopts, "dtb")
            if dtb:
                opts += [ "-dtb", dtb ]

        return opts

    def add_peripheral(self, peripheral):
        self._devices.append(peripheral)

    def start(self, qemu_dir_override="", dryrun=False, extras=[]):
        qemu_path = self.get_qemu_exec_name()
        qemu_path = os.path.join(qemu_dir_override, qemu_path)
        cmds = [
            qemu_path,
            *self.get_qemu_general_opts(),
            *self.get_qemu_arch_opts(),
            *self.get_qemu_debug_opts()
        ]

        for dev in self._devices:
            cmds += dev.get_qemu_opts()

        cmds += extras
        logger.info(" ".join(cmds))

        if dryrun:
            logger.info("[DRY RUN] QEMU not invoked")
            return
        
        handle = subprocess.Popen(cmds)
        logger.info(f"QEMU launched (pid={handle.pid})")
        
        while True:
            ret_code = handle.poll()
            if ret_code is not None:
                return ret_code
            time.sleep(5)

class QEMUx86Exec(QEMUExec):
    def __init__(self, options) -> None:
        super().__init__(options)

    def get_qemu_exec_name(self):
        if get_config(self._opt, "arch") in ["i386", "x86_32"]:
            return "qemu-system-i386"
        else:
            return "qemu-system-x86_64"

def main():
    global g_lookup

    arg = argparse.ArgumentParser()

    arg.add_argument("config_file")
    arg.add_argument("--qemu-dir", default="")
    arg.add_argument("--dry", action='store_true')
    arg.add_argument("-v", "--values", action='append', default=[])

    arg_opt, extras = arg.parse_known_args()

    opts = {}
    with open(arg_opt.config_file, 'r') as f:
        opts.update(json.loads(f.read()))
    
    for kv in arg_opt.values:
        splits = kv.split('=')
        k, v = splits[0], "=".join(splits[1:])
        g_lookup[k] = v

    arch = get_config(opts, "arch")

    q = None
    if arch in ["i386", "x86_32", "x86_64"]:
        q = QEMUx86Exec(opts)
    else:
        raise Exception(f"undefined arch: {arch}")
    
    extras = [ x for x in extras if x != '--']
    q.start(arg_opt.qemu_dir, arg_opt.dry, extras)

if __name__ == "__main__":
    main()
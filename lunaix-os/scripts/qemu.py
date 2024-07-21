#!/usr/bin/env python 

import subprocess, time, os, re, argparse, json
from pathlib import PurePosixPath

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

def parse_protocol(opt):
    protocol = get_config(opt, "protocol", "telnet")
    addr     = get_config(opt, "addr", ":12345")
    logfile  = get_config(opt, "logfile")

    return (f"{protocol}:{addr}", logfile)

class QEMUPeripherals:
    def __init__(self, name, opt) -> None:
        self.name = name
        self._opt = opt

    def get_qemu_opts(self) -> list:
        pass

class BasicSerialDevice(QEMUPeripherals):
    def __init__(self, opt) -> None:
        super().__init__("serial", opt)

    def get_qemu_opts(self):
        link, logfile = parse_protocol(self._opt)

        cmds = [ link, "server", "nowait" ]
        if logfile:
            cmds.append(f"logfile={logfile}")
        return [ "-serial", join_attrs(cmds) ]
    
class PCISerialDevice(QEMUPeripherals):
    def __init__(self, opt) -> None:
        super().__init__("pci-serial", opt)

    def get_qemu_opts(self):
        uniq = hex(self.__hash__())[2:]
        name = f"chrdev.{uniq}"
        cmds = [ "pci-serial", f"id=uart.{uniq}", f"chardev={name}" ]
        chrdev = [ "file", f"id={name}" ]
        
        logfile = get_config(self._opt, "logfile", required=True)
        chrdev.append(f"path={logfile}")

        return [ 
            "-chardev", join_attrs(chrdev),
            "-device", join_attrs(cmds)
         ]
    
class AHCIBus(QEMUPeripherals):
    def __init__(self, opt) -> None:
        super().__init__("ahci", opt)

    def get_qemu_opts(self):
        opt = self._opt
        name: str = get_config(opt, "name", required=True)
        name = name.strip().replace(" ", "_")
        cmds = [ "-device", f"ahci,id={name}" ]

        for i, disk in enumerate(get_config(opt, "disks", default=[])):
            d_type = get_config(disk, "type",   default="ide-hd")
            d_img  = get_config(disk, "img",    required=True)
            d_ro   = get_config(disk, "ro",     default=False)
            d_fmt  = get_config(disk, "format", default="raw")
            d_id   = f"disk_{i}"
            
            cmds += [
                "-drive", join_attrs([
                    f"id={d_id},"
                    f"file={d_img}",
                    f"readonly={'on' if d_ro else 'off'}",
                    f"if=none",
                    f"format={d_fmt}"
                ]),
                "-device", join_attrs([
                    d_type,
                    f"drive={d_id}",
                    f"bus={name}.{i}"
                ])
            ]
        
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
        link, logfile = parse_protocol(self._opt)

        return [
            "-monitor", join_attrs([
                link,
                "server",
                "nowait", 
                f"logfile={logfile}"
            ])
        ]

class QEMUExec:
    devices = {
        "basic_serial": BasicSerialDevice,
        "ahci": AHCIBus,
        "rtc": RTCDevice,
        "hmp": QEMUMonitor,
        "pci-serial": PCISerialDevice
    }

    def __init__(self, options) -> None:
        self._opt = options
        self._devices = []

        for dev in get_config(options, "devices", default=[]):
            dev_class = get_config(dev, "class")
            if dev_class not in QEMUExec.devices:
                raise Exception(f"device class: {dev_class} is not defined")
            
            self._devices.append(QEMUExec.devices[dev_class](dev))
    
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
        return [
            "-m", get_config(self._opt, "memory", required=True),
            "-smp", get_config(self._opt, "ncpu", default=1)
        ]

    def add_peripheral(self, peripheral):
        self._devices.append(peripheral)

    def start(self, qemu_dir_override=""):
        qemu_path = self.get_qemu_exec_name()
        qemu_path = os.path.join(qemu_dir_override, qemu_path)
        cmds = [
            qemu_path,
            *self.get_qemu_arch_opts(),
            *self.get_qemu_debug_opts()
        ]

        for dev in self._devices:
            cmds += dev.get_qemu_opts()

        print(" ".join(cmds), "\n")
        
        handle = subprocess.Popen(cmds)
        
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
    arg.add_argument("-v", "--values", action='append', default=[])

    arg_opt = arg.parse_args()

    opts = {}
    with open(arg_opt.config_file, 'r') as f:
        opts.update(json.loads(f.read()))
    
    for kv in arg_opt.values:
        [k, v] = kv.split('=')
        g_lookup[k] = v

    arch = get_config(opts, "arch")

    q = None
    if arch in ["i386", "x86_32", "x86_64"]:
        q = QEMUx86Exec(opts)
    else:
        raise Exception(f"undefined arch: {arch}")
    
    q.start(arg_opt.qemu_dir)

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(e)
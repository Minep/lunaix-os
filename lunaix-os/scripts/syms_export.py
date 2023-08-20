import subprocess
import math
import re

sym_line = re.compile(r"^(?P<addr>[0-9a-f]+)\s[a-z]\s+F\s+\.[a-z._]+\s+[0-9a-f]+(?P<label>.+)$")

def main(kbin, sym_out, endianness='little', bits=32, just_print=False):
    assert bits >= 32
    assert (1 << int(math.log2(bits))) == bits

    b_len = bits // 8
    result = subprocess.run(["objdump", "--syms", kbin],stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        print(result.stderr)
        return
    
    def to_native(val: int):
        return val.to_bytes(b_len, endianness, signed=False)
    
    output = result.stdout.split(b'\n')
    functions = []
    for l in output:
        l = l.decode("ascii")
        if not l:
            continue
        
        mo = sym_line.match(l)
        if not mo:
            continue

        mg = mo.groupdict()
        addr = int(mg["addr"], 16)
        label = mg["label"].strip() + '\0'

        functions.append((addr, label))

    functions = sorted(functions, key=lambda x: x[0])

    if just_print:
        for (a, l) in functions:
            print(hex(a), ":", l)
        return
    
    # struct {
    #   ptr_t addr;
    #   unsigned int label_off;
    # }
    sym_struct_sz = b_len * 2
    meta_struct_sz = 4 * 2
    label_off_base = sym_struct_sz * len(functions) + meta_struct_sz
    alignment = bytearray([b'\0' for _ in range(b_len - 4)])

    label_off = 0
    bmask = b_len - 1   
    text_region = bytearray()
    null = bytearray(b'\0')
    with open(sym_out, mode='wb') as f:
        # struct {
        #   unsigned int num_entry;
        #   unsigned int label_off_base;
        # }
        f.write(to_native(len(functions)))
        f.write(to_native(label_off_base))

        for a, l in functions:
            f.write(to_native(a))
            f.write(to_native(label_off))
            f.write(alignment)
            
            text_region += bytes(l, 'ascii')
            aligned_len = (len(l) + bmask) & ~bmask
            for i in range(aligned_len - len(l)):
                text_region.append(null[0])

            label_off += aligned_len
        
        f.write(text_region)

import argparse
import sys
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('elf_exec')
    parser.add_argument('-o', '--outfile', required=True)
    parser.add_argument('--bits', default=32)
    parser.add_argument('--order', default='little')
    parser.add_argument('-p', '--just-print', action='store_true')

    args = parser.parse_args()

    main(args.elf_exec, args.outfile, endianness=args.order, bits=int(args.bits), just_print=args.just_print)


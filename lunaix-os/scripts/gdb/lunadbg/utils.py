import gdb

def pid_argument(argument):
    if not argument:
        return "__current"
    else:
        return f"sched_ctx._procs[({argument})]"

def llist_foreach(head: gdb.Value, container_type: gdb.Type, field, cb):
    c = head
    i = 0
    offset = gdb.parse_and_eval('0').cast(container_type)[field].address
    voidp = gdb.lookup_type("void").pointer()
    offset_p = int(offset)
    while (c["next"] != head):
        next = gdb.Value(int(c["next"]) - offset_p)
        el = next.cast(container_type)
        cb(i, el)
        c = c["next"]
        i+=1
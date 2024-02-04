import gdb

def pid_argument(argument):
    if not argument:
        return "__current"
    else:
        return f"sched_ctx.procs[({argument})]"

def llist_foreach(head: gdb.Value, container_type: gdb.Type, field, cb, inclusive=True):
    c = head
    i = 0
    offset = gdb.Value(0).cast(container_type)[field].address
    offset_p = int(offset)

    if not inclusive:
        c = c["next"]

    while (True):
        current = gdb.Value(int(c) - offset_p)
        el = current.cast(container_type)
        cb(i, el)
        c = c["next"]
        i+=1
        if c == head:
            break

def get_dnode_name(dnode):
    return dnode['name']['value'].string()

def get_dnode_path(dnode):
    components = ['']
    current = dnode
    while (current != 0 and current != current['parent']):
        components.append(get_dnode_name(current))
        current = current['parent']
    components.append('')
    return '/'.join(reversed(components))
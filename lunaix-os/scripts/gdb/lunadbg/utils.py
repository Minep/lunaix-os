import gdb

def pid_argument(argument):
    if not argument:
        return "__current"
    else:
        return f"sched_ctx._procs[({argument})]"

def llist_foreach(head, container_type, cb):
    c = head
    i = 0
    while (c["next"] != head):
        el = c["next"].cast(container_type)
        cb(i, el)
        c = c["next"]
        i+=1
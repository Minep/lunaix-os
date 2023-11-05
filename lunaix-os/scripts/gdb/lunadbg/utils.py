
def pid_argument(argument):
    if not argument:
        return "__current"
    else:
        return f"sched_ctx._procs[({argument})]"
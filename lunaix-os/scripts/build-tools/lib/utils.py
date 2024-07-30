import os

def join_path(stem, path):
    if os.path.isabs(path):
        return path
    return os.path.join(stem, path)
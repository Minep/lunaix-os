class ConfigProvider:
    def __init__(self) -> None:
        pass

    def configured_value(self, name):
        raise ValueError(f"config '{name}' is undefined or disabled")
    
    def has_config(self, name):
        return False
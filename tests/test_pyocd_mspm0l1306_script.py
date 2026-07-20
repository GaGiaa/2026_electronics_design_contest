from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "tools" / "pyocd-mspm0l1306.py"


class FakeAccessPort:
    has_rom_table = False
    rom_addr = 0


class FakeTarget:
    def __init__(self):
        self.aps = {0: FakeAccessPort()}


class FakeDiscoverySequence:
    def __init__(self):
        self.after = {}

    def insert_after(self, name, task):
        self.after[name] = task
        return self


class FakeInitSequence:
    def __init__(self):
        self.wrapper = None

    def wrap_task(self, name, wrapper):
        assert name == "discovery"
        self.wrapper = wrapper


def test_script_applies_ti_rom_table_datapatch_after_ap_creation():
    assert SCRIPT.exists()
    spec = spec_from_file_location("mspm0l1306_patch", SCRIPT)
    module = module_from_spec(spec)
    spec.loader.exec_module(module)

    target = FakeTarget()
    init_sequence = FakeInitSequence()
    module.will_init_target(target, init_sequence)
    discovery = FakeDiscoverySequence()
    init_sequence.wrapper(discovery)
    _, apply_patch = discovery.after["create_aps"]
    apply_patch()

    assert target.aps[0].has_rom_table is True
    assert target.aps[0].rom_addr == 0xF0000000


if __name__ == "__main__":
    test_script_applies_ti_rom_table_datapatch_after_ap_creation()

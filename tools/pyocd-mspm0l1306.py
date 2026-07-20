"""Apply the MSPM0L1306 CMSIS-Pack ROM-table datapatch for pyOCD."""

ROM_TABLE_ADDRESS = 0xF0000000


def will_init_target(target, init_sequence):
    """Fix AP0 discovery before pyOCD scans its CoreSight ROM table."""

    def apply_rom_table_address():
        access_port = target.aps[0]
        access_port.has_rom_table = True
        access_port.rom_addr = ROM_TABLE_ADDRESS

    init_sequence.wrap_task(
        "discovery",
        lambda discovery: discovery.insert_after(
            "create_aps",
            ("apply_mspm0l1306_rom_table_address", apply_rom_table_address),
        ),
    )

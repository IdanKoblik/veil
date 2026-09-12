SECTIONS
{
    .commands : ALIGN(8)
    {
        __start_commands = .;
        KEEP(*(SORT_BY_NAME(commands.*)))
        __stop_commands = .;
    }
}
INSERT AFTER .data.rel.ro;

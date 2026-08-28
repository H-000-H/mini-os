/* default stack size */
__mini_os_stack_size = 0x800;     /* 2KB，按需调整 */

. = ALIGN(8);
__mini_os_heap_start = .;

. = ORIGIN(RAM) + LENGTH(RAM) - __mini_os_stack_size;
. = ALIGN(8);
__mini_os_heap_end = .;

_estack = ORIGIN(RAM) + LENGTH(RAM);

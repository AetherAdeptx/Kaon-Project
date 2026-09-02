#include <stddef.h>
#include <stdint.h>

#include "Config.h"
#include "Console.h"
#include "Keyboard.h"
#include "Librarian.h"
#include "serial.h"
#include "system.h"
#include "vga.h"

void kernel_main(uintptr_t boot_memory_base, size_t boot_memory_size,
                 uintptr_t managed_memory_base, size_t managed_memory_size)
{
    const LibrarianMemoryLayout memory_layout = {
        .boot_memory_base = boot_memory_base,
        .boot_memory_size = boot_memory_size,
        .managed_memory_base = managed_memory_base,
        .managed_memory_size = managed_memory_size
    };
    void *test_page;
    size_t test_page_index;
    volatile uint64_t *test_word;

    vga_initialize();
    serial_initialize();
    keyboard_initialize();
    console_initialize(KAON_CONSOLE_DRAW_ENABLED != 0);

    if (!librarian_initialize(&memory_layout)) {
        system_panic("Invalid boot memory layout");
    }

    test_page = librarian_allocate_page();
    if (test_page == NULL
        || (uintptr_t)test_page < librarian_get_assignment_start()
        || (uintptr_t)test_page >= librarian_get_assignment_end()
        || !librarian_get_page_index(test_page, &test_page_index)
        || librarian_get_physical_address(test_page_index)
            != (uintptr_t)test_page) {
        system_panic("Librarian page allocation failed");
    }

    test_word = (volatile uint64_t *)test_page;
    *test_word = UINT64_C(0x4B414F4E4D454D21);
    if (*test_word != UINT64_C(0x4B414F4E4D454D21)) {
        system_panic("Librarian memory test failed");
    }
    *test_word = 0;

    if (!librarian_release_page(test_page)) {
        system_panic("Librarian page release failed");
    }

    vga_write_line("Kaon C kernel is running.");
    serial_write_line("Kaon C kernel is running.");
    serial_write_line("Librarian: page tables and allocator ready.");
    serial_write_line("Keyboard: PS/2 polling ready.");

    system_run_forever();
}

_Noreturn void system_run_forever(void)
{
    for (;;) {
        keyboard_poll();
        console_update();
        __asm__ volatile ("pause");
    }
}

_Noreturn void system_idle_forever(void)
{
    /* Interrupts stay disabled until Kaon has an IDT and IRQ handlers. */
    __asm__ volatile ("cli");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

_Noreturn void system_panic(const char *message)
{
    vga_write("PANIC: ");
    vga_write_line(message);
    serial_write("PANIC: ");
    serial_write_line(message);
    system_idle_forever();
}

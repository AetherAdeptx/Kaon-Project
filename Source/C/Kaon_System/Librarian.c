#include "Config.h"
#include "Globals.h"
#include "Librarian.h"
#include "memory.h"

static LibrarianMemoryLayout memory_layout KAON_GLOBAL_VARIABLE;
static unsigned char *in_use_bitmap KAON_GLOBAL_VARIABLE;
static uintptr_t *physical_page_table KAON_GLOBAL_VARIABLE;
static size_t page_count KAON_GLOBAL_VARIABLE;
static size_t free_page_count KAON_GLOBAL_VARIABLE;
static size_t metadata_page_count KAON_GLOBAL_VARIABLE;
static size_t assignment_first_page KAON_GLOBAL_VARIABLE;
static size_t assignment_end_page KAON_GLOBAL_VARIABLE;
static bool is_initialized KAON_GLOBAL_VARIABLE;

static bool is_page_aligned(uintptr_t value)
{
    return value % LIBRARIAN_PAGE_SIZE == 0;
}

static bool page_is_marked(size_t page_index)
{
    const unsigned char mask =
        (unsigned char)(1U << (page_index % 8));

    return (in_use_bitmap[page_index / 8] & mask) != 0;
}

static void mark_page(size_t page_index, bool in_use)
{
    const unsigned char mask =
        (unsigned char)(1U << (page_index % 8));

    if (in_use) {
        in_use_bitmap[page_index / 8] |= mask;
    } else {
        in_use_bitmap[page_index / 8] &= (unsigned char)~mask;
    }
}

bool librarian_initialize(const LibrarianMemoryLayout *layout)
{
    uintptr_t boot_memory_end;
    size_t bitmap_size;
    size_t address_table_offset;
    size_t address_table_size;
    size_t metadata_size;
    size_t assignment_start_offset;
    size_t assignment_end_offset;

    if (layout == NULL || layout->boot_memory_size == 0
        || layout->managed_memory_size == 0) {
        return false;
    }

    if (!is_page_aligned(layout->boot_memory_base)
        || !is_page_aligned(layout->boot_memory_size)
        || !is_page_aligned(layout->managed_memory_base)
        || !is_page_aligned(layout->managed_memory_size)) {
        return false;
    }

    if (layout->boot_memory_base
        > UINTPTR_MAX - layout->boot_memory_size) {
        return false;
    }

    boot_memory_end = layout->boot_memory_base + layout->boot_memory_size;
    if (boot_memory_end != layout->managed_memory_base
        || layout->managed_memory_base
            > UINTPTR_MAX - layout->managed_memory_size) {
        return false;
    }

    page_count = layout->managed_memory_size / LIBRARIAN_PAGE_SIZE;
    if (page_count == 0 || page_count > (SIZE_MAX - 7)
        || page_count > SIZE_MAX / sizeof(*physical_page_table)) {
        return false;
    }

    bitmap_size = (page_count + 7) / 8;
    address_table_offset =
        (bitmap_size + sizeof(*physical_page_table) - 1)
        & ~(sizeof(*physical_page_table) - 1);
    address_table_size = page_count * sizeof(*physical_page_table);

    if (address_table_offset > SIZE_MAX - address_table_size) {
        return false;
    }

    metadata_size = address_table_offset + address_table_size;
    metadata_page_count =
        (metadata_size + LIBRARIAN_PAGE_SIZE - 1) / LIBRARIAN_PAGE_SIZE;
    if (metadata_page_count >= page_count) {
        return false;
    }

    assignment_start_offset = KAON_LIBRARIAN_ASSIGNMENT_START_OFFSET;
    assignment_end_offset = KAON_LIBRARIAN_ASSIGNMENT_END_OFFSET;

    if (assignment_start_offset == 0) {
        assignment_start_offset =
            metadata_page_count * LIBRARIAN_PAGE_SIZE;
    }
    if (assignment_end_offset == 0) {
        assignment_end_offset = layout->managed_memory_size;
    }

    if (!is_page_aligned(assignment_start_offset)
        || !is_page_aligned(assignment_end_offset)
        || assignment_start_offset
            < metadata_page_count * LIBRARIAN_PAGE_SIZE
        || assignment_start_offset >= assignment_end_offset
        || assignment_end_offset > layout->managed_memory_size) {
        return false;
    }

    assignment_first_page =
        assignment_start_offset / LIBRARIAN_PAGE_SIZE;
    assignment_end_page = assignment_end_offset / LIBRARIAN_PAGE_SIZE;

    memory_layout = *layout;
    in_use_bitmap = (unsigned char *)layout->managed_memory_base;
    physical_page_table = (uintptr_t *)(layout->managed_memory_base
                                        + address_table_offset);

    /* Reserved and out-of-bound pages begin marked; enable only the pool. */
    memset(in_use_bitmap, 0xFF, bitmap_size);
    for (size_t page_index = 0; page_index < page_count; ++page_index) {
        physical_page_table[page_index] = layout->managed_memory_base
            + page_index * LIBRARIAN_PAGE_SIZE;
    }

    for (size_t page_index = assignment_first_page;
         page_index < assignment_end_page;
         ++page_index) {
        mark_page(page_index, false);
    }

    free_page_count = assignment_end_page - assignment_first_page;
    is_initialized = true;
    return true;
}

const LibrarianMemoryLayout *librarian_get_memory_layout(void)
{
    return is_initialized ? &memory_layout : NULL;
}

void *librarian_allocate_page(void)
{
    return librarian_allocate_pages(1);
}

void *librarian_allocate_pages(size_t requested_page_count)
{
    size_t contiguous_page_count = 0;

    if (!is_initialized || requested_page_count == 0
        || requested_page_count > free_page_count) {
        return NULL;
    }

    for (size_t page_index = assignment_first_page;
         page_index < assignment_end_page;
         ++page_index) {
        if (page_is_marked(page_index)) {
            contiguous_page_count = 0;
            continue;
        }

        ++contiguous_page_count;
        if (contiguous_page_count == requested_page_count) {
            const size_t first_page_index =
                page_index + 1 - requested_page_count;

            for (size_t allocated_index = first_page_index;
                 allocated_index <= page_index;
                 ++allocated_index) {
                mark_page(allocated_index, true);
            }

            free_page_count -= requested_page_count;
            return (void *)physical_page_table[first_page_index];
        }
    }

    return NULL;
}

bool librarian_release_page(void *page)
{
    return librarian_release_pages(page, 1);
}

bool librarian_release_pages(void *first_page, size_t released_page_count)
{
    size_t first_page_index;

    if (released_page_count == 0
        || !librarian_get_page_index(first_page, &first_page_index)
        || first_page_index < assignment_first_page
        || first_page_index >= assignment_end_page
        || released_page_count
            > assignment_end_page - first_page_index) {
        return false;
    }

    for (size_t page_index = first_page_index;
         page_index < first_page_index + released_page_count;
         ++page_index) {
        if (!page_is_marked(page_index)) {
            return false;
        }
    }

    for (size_t page_index = first_page_index;
         page_index < first_page_index + released_page_count;
         ++page_index) {
        mark_page(page_index, false);
    }

    free_page_count += released_page_count;

    return true;
}

bool librarian_page_is_in_use(size_t page_index)
{
    return is_initialized && page_index < page_count
        && page_is_marked(page_index);
}

bool librarian_get_page_index(const void *address, size_t *page_index)
{
    uintptr_t numeric_address;
    uintptr_t region_end;

    if (!is_initialized || address == NULL || page_index == NULL) {
        return false;
    }

    numeric_address = (uintptr_t)address;
    region_end = memory_layout.managed_memory_base
        + memory_layout.managed_memory_size;

    if (!is_page_aligned(numeric_address)
        || numeric_address < memory_layout.managed_memory_base
        || numeric_address >= region_end) {
        return false;
    }

    *page_index =
        (numeric_address - memory_layout.managed_memory_base)
        / LIBRARIAN_PAGE_SIZE;
    return true;
}

uintptr_t librarian_get_physical_address(size_t page_index)
{
    if (!is_initialized || page_index >= page_count) {
        return UINTPTR_MAX;
    }

    return physical_page_table[page_index];
}

size_t librarian_get_page_count(void)
{
    return is_initialized ? page_count : 0;
}

size_t librarian_get_free_page_count(void)
{
    return is_initialized ? free_page_count : 0;
}

size_t librarian_get_metadata_page_count(void)
{
    return is_initialized ? metadata_page_count : 0;
}

uintptr_t librarian_get_assignment_start(void)
{
    if (!is_initialized) {
        return 0;
    }

    return memory_layout.managed_memory_base
        + assignment_first_page * LIBRARIAN_PAGE_SIZE;
}

uintptr_t librarian_get_assignment_end(void)
{
    if (!is_initialized) {
        return 0;
    }

    return memory_layout.managed_memory_base
        + assignment_end_page * LIBRARIAN_PAGE_SIZE;
}

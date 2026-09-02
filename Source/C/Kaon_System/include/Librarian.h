#ifndef KAON_LIBRARIAN_H
#define KAON_LIBRARIAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uintptr_t boot_memory_base;
    size_t boot_memory_size;
    uintptr_t managed_memory_base;
    size_t managed_memory_size;
} LibrarianMemoryLayout;

enum {
    LIBRARIAN_PAGE_SIZE = 4096
};

/* Initializes the page metadata inside the loader-supplied managed region. */
bool librarian_initialize(const LibrarianMemoryLayout *layout);
const LibrarianMemoryLayout *librarian_get_memory_layout(void);

void *librarian_allocate_page(void);
bool librarian_release_page(void *page);
void *librarian_allocate_pages(size_t requested_page_count);
bool librarian_release_pages(void *first_page, size_t released_page_count);
bool librarian_page_is_in_use(size_t page_index);
bool librarian_get_page_index(const void *address, size_t *page_index);
uintptr_t librarian_get_physical_address(size_t page_index);
size_t librarian_get_page_count(void);
size_t librarian_get_free_page_count(void);
size_t librarian_get_metadata_page_count(void);
uintptr_t librarian_get_assignment_start(void);
uintptr_t librarian_get_assignment_end(void);

#endif

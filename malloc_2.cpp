#include <iostream>
#include <unistd.h>
#include <string.h>

#define MAX_SIZE 100000000
using namespace std;

class MallocMetadata {
public:
    size_t size;
    bool is_free;
    MallocMetadata *next;
    MallocMetadata *prev;
};

MallocMetadata *block_list_base = nullptr;
MallocMetadata *block_list_top = nullptr;
size_t num_of_allocated_blocks = 0;
size_t num_of_free_blocks = 0;
size_t num_of_allocated_bytes = 0;
size_t num_of_free_bytes = 0;

MallocMetadata *request_block(MallocMetadata *last, size_t size) {
    MallocMetadata *meta_block;
    meta_block = (MallocMetadata *) (sbrk(sizeof(MallocMetadata) + size));
    if (meta_block == (void *) -1) {
        return nullptr;
    }
    meta_block->size = size;
    meta_block->is_free = false;
    meta_block->next = nullptr;
    meta_block->prev = last;
    if (block_list_top) {
        block_list_top->next = meta_block;
    }
    block_list_top = meta_block;

    num_of_allocated_blocks++;
    num_of_allocated_bytes += size;
    return meta_block;
}

MallocMetadata *find_fitting_block(size_t size) {
    MallocMetadata *curr = block_list_base;
    while (curr) {
        if (curr->size >= size && curr->is_free) {
            return curr;
        }
        curr = curr->next;
    }
    return nullptr;
}

void *smalloc(size_t size) {
    if (size == 0 || size > MAX_SIZE) {
        return nullptr;
    }
    MallocMetadata *requested;
    if (!block_list_base) {
        //nothing was allocated
        requested = request_block(nullptr, size);
        if (!requested) {
            return nullptr;
        }
        block_list_base = requested;
    } else {
        requested = find_fitting_block(size);
        if (!requested) {
            requested = request_block(block_list_top, size);
            if (!requested) {
                return nullptr;
            }
        } else {
            requested->is_free = false;
            num_of_free_blocks--;
            num_of_free_bytes -= requested->size;
        }
    }
    return requested + 1;
}

void sfree(void *p) {
    if (!p) {
        return;
    }

    MallocMetadata *curr = (MallocMetadata *) p - 1;
    if (curr->is_free) {
        return;
    }
    curr->is_free = true;
    num_of_free_blocks++;
    num_of_free_bytes += curr->size;
}

void *scalloc(size_t num, size_t size) {
    void *block = smalloc(num * size);
    if (!block) {
        return nullptr;
    }
    memset(block, 0, num * size);
    return block;
}

void *srealloc(void *oldp, size_t size) {
    if (!oldp) {
        return smalloc(size);
    }
    MallocMetadata *curr = (MallocMetadata *) oldp - 1;
    if (curr->size >= size) {
        return oldp;
    } else {
        void *new_block = smalloc(size);
        if (!new_block) {
            return nullptr;
        }
        memcpy(new_block, oldp, curr->size);
        sfree(oldp);
        return new_block;
    }
}

size_t _num_free_blocks() {
    return num_of_free_blocks;
}

size_t _num_free_bytes() {
    return num_of_free_bytes;
}

size_t _num_allocated_blocks() {
    return num_of_allocated_blocks;
}

size_t _num_allocated_bytes() {
    return num_of_allocated_bytes;
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * sizeof(MallocMetadata);
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}

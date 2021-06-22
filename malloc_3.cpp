#include <iostream>
#include <unistd.h>
#include <list>
#define MAX_SIZE 100000000
using namespace std;
class MallocMetadata {
 public:
  size_t size;
  bool is_free;
  MallocMetadata *next;
  MallocMetadata *prev;
};

MallocMetadata *block_list_bottom = nullptr;
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
    block_list_top ? block_list_top->next = meta_block : block_list_top = meta_block;

    num_of_allocated_blocks++;
    num_of_allocated_bytes += size;
    return meta_block;
}
MallocMetadata *find_fitting_block(size_t size) {
    MallocMetadata *curr = block_list_bottom;
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
    if (!block_list_bottom) {
        //nothing was allocated
        requested = request_block(nullptr, size);
        if (!requested) {
            return nullptr;
        }
        block_list_bottom = requested;
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
    return requested + sizeof(MallocMetadata);
}

void sfree(void *p) {
    if (!p) {
        return;
    }
    MallocMetadata *curr = block_list_bottom;
    while (curr) {
        if (curr + sizeof(MallocMetadata) == p) {
            curr->is_free = true;
            num_of_free_blocks++;
            num_of_free_bytes -= curr->size;
            return;
        }
        curr = curr->next;
    }
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
    MallocMetadata *curr = block_list_bottom;
    while (curr) {
        if (curr + sizeof(MallocMetadata) == oldp) {
            if (curr->size >= size) {
                return oldp;
            } else {
                void *new_block = smalloc(size);
                if (!new_block) {
                    return nullptr;
                }
                sfree(oldp);
                return new_block;
            }
        }
        curr = curr->next;
    }
    //shouldn't reach to this
    return nullptr;
}

size_t _num_free_blocks() {
    return num_of_free_blocks;
}
size_t _num_free_bytes() {
    return num_of_free_bytes;
}
size_t _num_allocated_blocks() {
    return num_of_allocated_blocks + num_of_free_blocks;
}
size_t _num_allocated_bytes() {
    return num_of_allocated_bytes + num_of_free_bytes;
}
size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * sizeof(MallocMetadata);
}
size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}

int main() {
    printf("%lu\n", _size_meta_data());
    int *arr = (int *) scalloc(1, 10);
    printf("%p\n", arr);
    sfree(arr);
    void *ptr_2 = srealloc(nullptr, 3);
    printf("%p\n", ptr_2);
    printf("%p\n", srealloc(ptr_2, 3));
    printf("%p\n", srealloc(ptr_2, 12));

    return 0;
}

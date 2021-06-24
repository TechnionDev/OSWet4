#include <iostream>
#include <unistd.h>
#include <list>

#define MAX_SIZE 100000000
#define KB 1024
#define NUM_OF_BUCKETS 128
#define EXCEPTION(name)                                  \
    class name : public MallocException {               \
    public:                                              \
        name(std::string str) : MallocException(str){}; \
    }


using namespace std;

class MallocException : public runtime_error {
public:
    MallocException(string str) : runtime_error(str) {};
};

EXCEPTION(StillAllocatedException);

class MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata *prev;
    // TODO: change to bitfield to save space

public:
    size_t getSize(void) {
        return this->size;
    }

    void setSize(size_t size) {
        this->size = size;
    }

    bool isFree(void) {
        return this->is_free;
    }

    void setAllocated(void) {
        this->is_free = false;
    }

    MallocMetadata *getPrev(void) {
        return this->prev;
    }

    void setPrev(MallocMetadata *prev) {
        this->prev = prev;
    }

    MallocMetadata *getNextInHeap(void) {
        return (MallocMetadata *) (((char *) this) + sizeof(MallocMetadata) + this->size);
    }

    /**
     * Sets the next block in the **bucket**. Should only be used when the block is free
     * @param next The next block in the bucket
     */
    void setNextBucketBlock(MallocMetadata *next) {
        if (not this->isFree()) {
            throw StillAllocatedException("Can't set the next bucket block while the block for an allocated block");
        }
        *(MallocMetadata **) (this + 1) = next;
    }

    MallocMetadata *getNextBucketBlock(void) {
        if (not this->isFree()) {
            throw StillAllocatedException("Can't get the next bucket block of an allocated block");
        }
        return *(MallocMetadata **) (this + 1);
    }
};

class Bucket {
    MallocMetadata *listHead;
    MallocMetadata *listTail;

public:
    Bucket() : listHead(nullptr), listTail(nullptr) {};

    void addBlock(MallocMetadata *block);

    MallocMetadata *acquireBlock(size_t size);
};

static Bucket buckets[NUM_OF_BUCKETS] = {Bucket()};
static MallocMetadata *page_block_head = nullptr;
static MallocMetadata *page_block_tail = nullptr;
static size_t num_of_allocated_blocks = 0;
static size_t num_of_free_blocks = 0;
static size_t num_of_allocated_bytes = 0;
static size_t num_of_free_bytes = 0;


void Bucket::addBlock(MallocMetadata *block) {
    if (not block->isFree()) {
        throw StillAllocatedException("Can't add an allocated block to bucket");
    } else if (this->listHead == nullptr) {
        assert(this->listTail == nullptr);
        this->listHead = this->listTail = block;
        block->setNextBucketBlock(nullptr);
        return;
    }

    // TODO: Maybe add validation for the correct block size. Probably not really required

    MallocMetadata *curr = this->listHead;
    MallocMetadata *next;
    if (curr->getSize() > block->getSize()) {
        block->setNextBucketBlock(this->listHead);
        this->listHead = block;
        return;
    }
    while ((next = curr->getNextBucketBlock()) != nullptr) {
        if (next->getSize() > block->getSize()) {
            block->setNextBucketBlock(next);
            curr->setNextBucketBlock(block);
            return;
        }
        curr = next;
    }

    // Bigger than anything in the bucket
    curr->setNextBucketBlock(block);
    block->setNextBucketBlock(nullptr);
    this->listTail = block;
}

MallocMetadata *Bucket::acquireBlock(size_t size) {
    MallocMetadata *curr = this->listHead;
    MallocMetadata *next;
    if (this->listHead->getSize() >= size) {
        this->listHead = curr->getNextBucketBlock();
        if (curr == this->listTail) {
            // If the head is also tail, then the list should be set to empty. Head was already set (next == nullptr)
            this->listTail = nullptr;
        }
        return curr;
    }
    while ((next = curr->getNextBucketBlock()) != nullptr) {
        if (next->getSize() >= size) {
            curr->setNextBucketBlock(next->getNextBucketBlock());
            // TODO: Split block and add the remainder to the correct bucket
            if (next == this->listTail) {
                this->listTail = curr;
            }
            return next;
        }
        curr = next;
    }

}


/**
 * Increases the page break to create a new block. Adds it to the general
 * @param size
 * @return
 */
MallocMetadata *request_block(size_t size) {
    MallocMetadata *meta_block;
    meta_block = (MallocMetadata *) (sbrk(sizeof(MallocMetadata) + size));
    if (meta_block == (void *) -1) {
        return nullptr;
    }
    meta_block->setSize(size);
    meta_block->setAllocated();
    meta_block->setPrev(page_block_tail);
    if (page_block_head) {
        page_block_head->next = meta_block;
    } else {
        lock_list_top = meta_block;
    }

    num_of_allocated_blocks++;
    num_of_allocated_bytes += size;
    return meta_block;
}

void *smalloc(size_t size) {
    if (size == 0 || size > MAX_SIZE) {
        return nullptr;
    }
    if (size >= NUM_OF_BUCKETS) {
        // TODO: Big allocation
        return nullptr;
    }

    // TODO: Reimplement this to fit the new buckets thingy
    MallocMetadata *requested;
    if (!block_list_tail) {
        // Nothing was allocated
        requested = request_block(nullptr, size);
        if (!requested) {
            return nullptr;
        }
        block_list_tail = requested;
    } else {
        requested = buckets[size / KB].acquireBlock(size);
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
    MallocMetadata *curr = ((MallocMetadata *) p) - 1;
    curr->is_free = true;
    num_of_free_blocks++;
    num_of_free_bytes -= curr->size;
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
    MallocMetadata *curr = block_list_tail;
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
    void *ptr = smalloc(10);
    printf("%p\n", ptr);
    sfree(ptr);
    void *ptr_2 = srealloc(nullptr, 3);
    printf("%p\n", ptr_2);
    printf("%p\n", srealloc(ptr_2, 3));
    printf("%p\n", srealloc(ptr_2, 12));

    return 0;
}

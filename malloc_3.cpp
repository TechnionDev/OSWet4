#include <iostream>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>

#define MAX_SIZE 100000000
#define KB 1024
#define NUM_OF_BUCKETS 128
#define MIN_SPLIT_BLOCK_SIZE_BYTES 128
#define MIN_SIZE 24
// TODO: Replace macro with this: min(((X) / NUM_OF_BUCKETS / KB), NUM_OF_BUCKETS - 1)
#define SIZE_TO_BUCKET(X) ((X) / NUM_OF_BUCKETS / KB)
#define EXCEPTION(name)                                  \
    class name : public MallocException {                \
    public:                                              \
        name(std::string str) : MallocException(str){};  \
    }

#define max(first, second) ((first) > (second) ? (first) : (second))

using namespace std;

static size_t num_of_allocated_blocks = 0;
static size_t num_of_free_blocks = 0;
static size_t num_of_allocated_bytes = 0;
static size_t num_of_free_bytes = 0;

class MallocException : public runtime_error {
public:
    explicit MallocException(const string &str) : runtime_error(str) {};
};

EXCEPTION(StillAllocatedException);

EXCEPTION(InvalidForMmapAllocations);

class MallocMetadata {
    struct {
        int is_free: 1;
        int is_mmap: 1;
    } flags;
    size_t size;
    MallocMetadata *prev_in_heap;
    MallocMetadata *next_bucket_block;
    MallocMetadata *prev_bucket_block;
    void *bucket_ptr;
    // TODO: change to bitfield to save space


    /**
     * Private function used to merge a recently freed block with its adjacent neighbours.
     * Be warned:
     * The function may destroy `this` so after using this function, you cannot rely on any of the object's data and should definitely not use any other functions of this object.
     */
    void mergeWithAdjacent();

public:

    /**
     * This function allocates a newly created object.
     *
     * !! It should **not** be used on previously allocated objects as it would mess up the stats !!
     * @param new_size The size of the block (that is exposed to the user)
     * @param new_prev The previous block in the heap
     * @param new_is_free Whether to allocate the block as free memory or not
     * @param is_mmap Whether the block is from sbrk or a memory mapping (aka mmap)
     */
    void init(size_t new_size, MallocMetadata *new_prev, bool new_is_free, bool is_mmap);

    size_t getSize() const {
        return this->size;
    }

    void setSize(size_t new_size) {
        if (this->isFree()) {
            num_of_free_bytes -= this->size - new_size;
        } else {
            num_of_allocated_bytes -= this->size - new_size;
        }
        this->size = new_size;
    }

    bool isFree() const {
        return this->flags.is_free;
    }

    void setFree();

    void setAllocated() {
        if (this->flags.is_mmap) {
            throw InvalidForMmapAllocations("Can't set an mmap allocated block as allocated (you should init the block as allocated)");
        }
        if (not this->isFree()) {
            throw StillAllocatedException("Can't allocate a block which is already allocated");
        }
        num_of_free_blocks--;
        num_of_allocated_blocks++;
        num_of_free_bytes -= this->getSize();
        num_of_allocated_bytes += this->getSize();
        this->flags.is_free = false;
    }

    MallocMetadata *getPrevInHeap() {
        if (this->flags.is_mmap) {
            return nullptr;
        }
        return this->prev_in_heap;
    }

    MallocMetadata *getNextInHeap();

    void removeSelfFromBucketChain();

    /**
     * Sets the next block in the **bucket**. Should only be used when the block is free
     * @param next The next block in the bucket
     */
    void setNextBucketBlock(MallocMetadata *next) {
        if (this->flags.is_mmap) {
            throw InvalidForMmapAllocations("Can't set the next bucket block for mmap block");
        }
        if (not this->isFree()) {
            throw StillAllocatedException("Can't set the next bucket block while the block for an allocated block");
        }
        this->next_bucket_block = next;
        if (next) {
            next->prev_bucket_block = this;
        }
    }

    void setPrevBucketBlock(MallocMetadata *new_prev) {
        if (this->flags.is_mmap) {
            throw InvalidForMmapAllocations("Can't set the previous bucket block for mmap block");
        }
        if (not this->isFree()) {
            throw StillAllocatedException("Can't set the new_prev bucket block while the block for an allocated block");
        }
        this->prev_bucket_block = new_prev;
        if (new_prev) {
            new_prev->next_bucket_block = this;
        }
    }

    MallocMetadata *getNextBucketBlock() {
        if (this->flags.is_mmap) {
            throw InvalidForMmapAllocations("Can't get the next bucket block for mmap block");
        }
        if (not this->isFree()) {
            throw StillAllocatedException("Can't get the next bucket block of an allocated block");
        }
        return this->next_bucket_block;
    }

    MallocMetadata *getPrevBucketBlock() {
        if (this->flags.is_mmap) {
            throw InvalidForMmapAllocations("Can't get the previous bucket block for mmap block");
        }
        if (not this->isFree()) {
            throw StillAllocatedException("Can't get the prev_in_heap bucket block of an allocated block");
        }
        return this->prev_bucket_block;
    }

    void *getBucketPtr() {
        if (this->flags.is_mmap) {
            throw InvalidForMmapAllocations("Can't get the bucket for an mmap block");
        }
        if (not this->isFree()) {
            throw StillAllocatedException("Can't get the bucket of an allocated block");
        }
        return this->bucket_ptr;
    }

    void setBucketPtr(void *bucket) {
        if (this->flags.is_mmap) {
            throw InvalidForMmapAllocations("Can't set the bucket for an mmap block");
        }
        if (not this->isFree()) {
            throw StillAllocatedException("Can't set the bucket of an allocated block");
        }
        this->bucket_ptr = bucket;
    }

    bool isMmap() const {
        return this->flags.is_mmap;
    }

    void destroy();
};

class Bucket {
    MallocMetadata *list_head;
    MallocMetadata *list_tail;

    friend void MallocMetadata::removeSelfFromBucketChain();

public:
    Bucket() : list_head(nullptr), list_tail(nullptr) {};

    void addBlock(MallocMetadata *block);

    MallocMetadata *acquireBlock(size_t size);
};

static Bucket buckets[NUM_OF_BUCKETS] = {Bucket()};
static MallocMetadata *page_block_head = nullptr;
static MallocMetadata *page_block_tail = nullptr;


void MallocMetadata::destroy() {
    if (this->flags.is_free) {
        num_of_free_bytes -= this->size;
        num_of_free_blocks--;
    } else {
        num_of_allocated_bytes -= this->size;
        num_of_allocated_blocks--;
    }
    MallocMetadata *next = this->getNextInHeap();
    if (next) {
        next->prev_in_heap = this->prev_in_heap;
    }
    if (this == page_block_tail) {
        page_block_tail = this->prev_in_heap;
    }
    this->size = 0;
    this->prev_bucket_block = this->prev_in_heap = this->next_bucket_block = nullptr;
}

void MallocMetadata::setFree() {
    num_of_allocated_blocks--;
    num_of_free_blocks++;
    num_of_free_bytes += this->getSize();
    num_of_allocated_bytes -= this->getSize();
    this->flags.is_free = true;
    this->mergeWithAdjacent();
    // The state of `this` is undefined after using mergeWithAdjacent
}

MallocMetadata *MallocMetadata::getNextInHeap() {
    if (this >= page_block_tail || this->flags.is_mmap) {
        return nullptr;
    }
    return (MallocMetadata *) (((char *) this) + sizeof(MallocMetadata) + this->size);
}

void MallocMetadata::init(size_t new_size, MallocMetadata *new_prev, bool new_is_free, bool is_mmap = false) {
    if (new_is_free) {
        num_of_free_bytes += new_size;
        num_of_free_blocks++;
    } else {
        num_of_allocated_bytes += new_size;
        num_of_allocated_blocks++;
    }
    this->flags.is_free = new_is_free;
    this->flags.is_mmap = is_mmap;
    this->size = new_size;
    if (!is_mmap) {
        this->prev_in_heap = new_prev;
        if (this > page_block_tail) {
            page_block_tail = this;
        } else {
            this->prev_in_heap = this->prev_bucket_block = this->next_bucket_block = nullptr;
        }
        if (this->getNextInHeap()) {
            this->getNextInHeap()->prev_in_heap = this;
        }
    }
}

void Bucket::addBlock(MallocMetadata *block) {
    if (block->isMmap()) {
        throw InvalidForMmapAllocations("Can't add to bucket a block that was allocated using mmap");
    }
    if (not block->isFree()) {
        throw StillAllocatedException("Can't add an allocated block to bucket");
    }
    block->setBucketPtr(this);

    if (this->list_head == nullptr) {
        this->list_head = this->list_tail = block;
        block->setNextBucketBlock(nullptr);
        block->setPrevBucketBlock(nullptr);
        return;
    }

    MallocMetadata *curr = this->list_head;
    MallocMetadata *next;
    if (curr->getSize() > block->getSize()) {
        block->setNextBucketBlock(this->list_head);
        block->setPrevBucketBlock(nullptr);
        this->list_head = block;
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
    this->list_tail = block;
}

MallocMetadata *Bucket::acquireBlock(size_t size) {
    MallocMetadata *curr = this->list_head;
    MallocMetadata *next;
    if (this->list_head == nullptr) {
        return nullptr;
    }

    if (this->list_head->getSize() >= size) {
        this->list_head = curr->getNextBucketBlock();
        if (curr == this->list_tail) {
            // If the head is also tail, then the list should be set to empty. Head was already set (next == nullptr)
            this->list_tail = nullptr;
        }
        curr->setBucketPtr(nullptr);
        // Check if the block needs splitting
        if (curr->getSize() - size >= sizeof(MallocMetadata) + MIN_SPLIT_BLOCK_SIZE_BYTES) {
            size_t leftover_size = curr->getSize() - sizeof(MallocMetadata) - size;
            curr->setSize(size);
            // Split the block and add the leftover to the current bucket
            auto *leftover = (MallocMetadata *) ((char *) (curr + 1) + size);
            leftover->init(leftover_size, curr, true);
            buckets[SIZE_TO_BUCKET(leftover_size)].addBlock(leftover);
        }
        return curr;
    }

    while ((next = curr->getNextBucketBlock()) != nullptr) {
        if (next->getSize() >= size) {
            curr->setNextBucketBlock(next->getNextBucketBlock());
            if (next == this->list_tail) {
                this->list_tail = curr;
            }
            curr = next;
            // Check if the block needs splitting
            if (curr->getSize() - size >= sizeof(MallocMetadata) + MIN_SPLIT_BLOCK_SIZE_BYTES) {
                size_t leftover_size = curr->getSize() - sizeof(MallocMetadata) - size;
                curr->setSize(size);
                // Split the block and add the leftover to the current bucket
                auto *leftover = (MallocMetadata *) ((char *) (curr + 1) + size);
                leftover->init(leftover_size, curr, true);
                buckets[SIZE_TO_BUCKET(leftover_size)].addBlock(leftover);
            }
            curr->setBucketPtr(nullptr);
            return curr;
        }
        curr = next;
    }

    return nullptr;
}

void MallocMetadata::mergeWithAdjacent() {
    MallocMetadata *adjacent;
    // Try merge with adjacent free blocks
    if ((adjacent = this->getNextInHeap())) {
        if (adjacent->isFree()) {
            if (adjacent == page_block_tail) {
                page_block_tail = this;
            }
            adjacent->removeSelfFromBucketChain();
            this->setSize(this->getSize() + adjacent->getSize() + sizeof(MallocMetadata));
            adjacent->destroy();
            this->removeSelfFromBucketChain();
        }
    }
    if (this != page_block_head) {
        adjacent = this->getPrevInHeap();
        if (adjacent->isFree()) {
            if (this == page_block_tail) {
                page_block_tail = adjacent;
            }
            this->removeSelfFromBucketChain();
            adjacent->setSize(adjacent->getSize() + this->getSize() + sizeof(MallocMetadata));
            this->destroy();
            // Note that from now and on `this` is not defined. Take care....
            adjacent->removeSelfFromBucketChain();
            buckets[SIZE_TO_BUCKET(adjacent->getSize())].addBlock(adjacent);
            return;
        }
    }
    buckets[SIZE_TO_BUCKET(this->getSize())].addBlock(this);
}

void MallocMetadata::removeSelfFromBucketChain() {
    if (this->flags.is_mmap) {
        throw InvalidForMmapAllocations("Can't remove blocks that were allocated using mmap from bucket");
    }
    if (not this->isFree()) {
        throw StillAllocatedException(
                "Can't remove a block which isn't free from bucket chain. The block can't possibly be in a bucket chain");
    }
    MallocMetadata *prev = this->getPrevBucketBlock();
    MallocMetadata *next = this->getNextBucketBlock();
    if (prev) {
        prev->setNextBucketBlock(next);
    } else if (next) {
        next->setNextBucketBlock(prev);
    }
    this->next_bucket_block = this->prev_bucket_block = nullptr;
    auto *bucket = (Bucket *) this->getBucketPtr();
    if (bucket) {
        if (bucket->list_head == this) {
            bucket->list_head = next;
        }
        if (bucket->list_tail == this) {
            bucket->list_tail = prev;
        }
        this->setBucketPtr(nullptr);
    }
}

/**
 * Increases the page break to create a new block. Adds it to the general
 * @param size
 * @return
 */
MallocMetadata *request_block(size_t size) {
    MallocMetadata *meta_block;
    if (page_block_tail and page_block_tail->isFree()) {
        meta_block = (MallocMetadata *) (sbrk(size - page_block_tail->getSize()));
        if (meta_block == (void *) -1) {
            return nullptr;
        }
        meta_block = page_block_tail;
        meta_block->removeSelfFromBucketChain();
        meta_block->setSize(size);
        meta_block->setAllocated();
        return meta_block;
    } else {
        meta_block = (MallocMetadata *) (sbrk(sizeof(MallocMetadata) + size));
        if (meta_block == (void *) -1) {
            return nullptr;
        }
        meta_block->init(size, page_block_tail, false);
        if (!page_block_head) {
            page_block_head = meta_block;
        }
        page_block_tail = meta_block;
        return meta_block;
    }
}

void *smalloc(size_t size) {
    if (size == 0 || size > MAX_SIZE) {
        return nullptr;
    }
    if (size >= KB * NUM_OF_BUCKETS) {
        auto *p = (MallocMetadata *) mmap(nullptr,
                                          size + sizeof(MallocMetadata),
                                          PROT_EXEC | PROT_READ | PROT_WRITE,
                                          MAP_ANONYMOUS | MAP_PRIVATE,
                                          -1,
                                          0);
        p->init(size, nullptr, false, true);
        return p + 1;
    }

    MallocMetadata *requested = nullptr;
    if (!page_block_tail) {
        // Nothing was allocated
        requested = request_block(size);
        if (!requested) {
            return nullptr;
        }
    } else {
        for (int i = SIZE_TO_BUCKET(size); i < NUM_OF_BUCKETS; i++) {
            if ((requested = buckets[i].acquireBlock(size)) != nullptr) {
                break;
            }
        }

        if (!requested) {
            requested = request_block(size);
            if (!requested) {
                return nullptr;
            }
        } else {
            requested->setAllocated();
        }
    }
    return requested + 1;
}

void sfree(void *p) {
    if (!p) {
        return;
    }
    MallocMetadata *curr = ((MallocMetadata *) p) - 1;
    if (curr->getSize() >= KB * NUM_OF_BUCKETS) {
        size_t old_size = curr->getSize();
        curr->destroy();
        munmap(curr, old_size + sizeof(MallocMetadata));
        return;
    }
    curr->setFree();
}

void *scalloc(size_t num, size_t size) {
    void *block = smalloc(num * size);
    if (not block) {
        return nullptr;
    }
    memset(block, 0, num * size);
    return block;
}

void *srealloc(void *oldp, size_t size) {
    if (!oldp) {
        return smalloc(size);
    }
    if (size == 0 || size > MAX_SIZE) {
        return nullptr;
    }

    MallocMetadata *curr = (MallocMetadata *) oldp - 1;
    if (size >= KB * NUM_OF_BUCKETS) {
        auto *new_block = (MallocMetadata *) mmap(nullptr,
                                                  size + sizeof(MallocMetadata),
                                                  PROT_EXEC | PROT_READ | PROT_WRITE,
                                                  MAP_ANONYMOUS | MAP_PRIVATE,
                                                  -1,
                                                  0);
        new_block->init(size, nullptr, false, true);
        size_t old_size = curr->getSize();
        curr->destroy();
        memmove(new_block + 1, oldp, min(old_size, size));
        munmap(curr, old_size + sizeof(MallocMetadata));
        return new_block + 1;
    }
    // Keep the same location
    if (curr->getSize() >= size) {
        if (curr->getSize() >= MIN_SPLIT_BLOCK_SIZE_BYTES + sizeof(MallocMetadata) + size) {
            size_t leftover_size = curr->getSize() - sizeof(MallocMetadata) - size;
            curr->setSize(size);
            // Split the block and add the leftover to the current bucket
            auto *leftover = (MallocMetadata *) ((char *) oldp + size);
            leftover->init(leftover_size, curr, true);
            buckets[SIZE_TO_BUCKET(leftover_size)].addBlock(leftover);
        }
        return oldp;
    }
    MallocMetadata *prev = curr->getPrevInHeap();
    MallocMetadata *next = curr->getNextInHeap();

    // Check for merge-able adjacent block
    if (prev and prev->isFree() and prev->getSize() + curr->getSize() >= size) {
        //merge with only the prev_in_heap block
        prev->removeSelfFromBucketChain();
        prev->setAllocated();
        prev->setSize(prev->getSize() + curr->getSize() + sizeof(MallocMetadata));
        size_t curr_size = curr->getSize();
        curr->destroy();
        memmove(prev + 1, oldp, curr_size);
        if (prev->getSize() >= MIN_SPLIT_BLOCK_SIZE_BYTES + sizeof(MallocMetadata) + size) {
            size_t leftover_size = prev->getSize() - sizeof(MallocMetadata) - size;
            prev->setSize(size);
            // Split the block and add the leftover to the current bucket
            auto *leftover = (MallocMetadata *) ((char *) (prev + 1) + size);
            leftover->init(leftover_size, prev, true);
            buckets[SIZE_TO_BUCKET(leftover_size)].addBlock(leftover);
        }
        return prev + 1;
    } else if (next and next->isFree() and next->getSize() + curr->getSize() >= size) {
        //merge with only the next block
        next->removeSelfFromBucketChain();
        curr->setSize(curr->getSize() + next->getSize() + sizeof(MallocMetadata));
        next->destroy();
        if (curr->getSize() >= MIN_SPLIT_BLOCK_SIZE_BYTES + sizeof(MallocMetadata) + size) {
            size_t leftover_size = curr->getSize() - sizeof(MallocMetadata) - size;
            curr->setSize(size);
            // Split the block and add the leftover to the current bucket
            auto *leftover = (MallocMetadata *) ((char *) (curr + 1) + size);
            leftover->init(leftover_size, curr, true);
            buckets[SIZE_TO_BUCKET(leftover_size)].addBlock(leftover);
        }
        return curr + 1;
    } else if (curr != page_block_tail and prev and next->isFree() and prev->isFree()
               and prev->getSize() + next->getSize() + curr->getSize() >= size) {
        //merge with the next and prev_in_heap block
        next->removeSelfFromBucketChain();
        prev->removeSelfFromBucketChain();
        prev->setAllocated();
        prev->setSize(prev->getSize() + curr->getSize() + next->getSize() + 2 * sizeof(MallocMetadata));
        size_t curr_size = curr->getSize();
        curr->destroy();
        next->destroy();
        memmove(prev + 1, oldp, curr_size);
        if (prev->getSize() >= MIN_SPLIT_BLOCK_SIZE_BYTES + sizeof(MallocMetadata) + size) {
            size_t leftover_size = prev->getSize() - sizeof(MallocMetadata) - size;
            prev->setSize(size);
            // Split the block and add the leftover to the current bucket
            auto *leftover = (MallocMetadata *) ((char *) (prev + 1) + size);
            leftover->init(leftover_size, prev, true);
            buckets[SIZE_TO_BUCKET(leftover_size)].addBlock(leftover);
        }
        return prev + 1;
    } else if (curr == page_block_tail) {
        sbrk(size - curr->getSize());
        curr->setSize(size);
        return oldp;
    } else {
        //allocate an entirely new block, and free the old block
        void *new_addr = smalloc(size);
        if (!new_addr) {
            return nullptr;
        }
        memmove(new_addr, oldp, curr->getSize());
        sfree(oldp);
        return new_addr;
    }

    //shouldn't reach to this
    return nullptr;
}
//TODO:: go over all of the functions and check if the statistics are updated

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

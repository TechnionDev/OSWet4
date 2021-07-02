#include <iostream>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

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
    MallocException(string str) : runtime_error(str) {};
};

EXCEPTION(StillAllocatedException);

class MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata *prev;
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
     */
    void init(size_t new_size, MallocMetadata *new_prev, bool new_is_free) {
        if (new_is_free) {
            num_of_free_bytes += new_size;
            num_of_free_blocks++;
        } else {
            num_of_allocated_bytes += new_size;
            num_of_allocated_blocks++;
        }
        this->is_free = new_is_free;
        this->prev = new_prev;
        this->size = new_size;
    }

    size_t getSize() {
        return this->size;
    }

    void setSize(size_t size) {
        if (this->isFree()) {
            num_of_free_bytes -= this->size - size;
        } else {
            num_of_allocated_bytes -= this->size - size;
        }
        this->size = size;
    }

    bool isFree() {
        return this->is_free;
    }

    void setFree();

    void setAllocated() {
        if (not this->isFree()) {
            throw StillAllocatedException("Can't allocate a block which is already allocated");
        }
        num_of_free_blocks--;
        num_of_allocated_blocks++;
        num_of_free_bytes -= this->getSize();
        num_of_allocated_bytes += this->getSize();
        this->is_free = false;
    }

    MallocMetadata *getPrevInHeap() {
        return this->prev;
    }

    MallocMetadata *getNextInHeap();

    void removeSelfFromBucketChain();

    /**
     * Sets the next block in the **bucket**. Should only be used when the block is free
     * @param next The next block in the bucket
     */
    void setNextBucketBlock(MallocMetadata *next) {
        if (not this->isFree()) {
            throw StillAllocatedException("Can't set the next bucket block while the block for an allocated block");
        }
        this->next_bucket_block = next;
        if (next) {
            next->prev_bucket_block = this;
        }
    }

    void setPrevBucketBlock(MallocMetadata *prev) {
        if (not this->isFree()) {
            throw StillAllocatedException("Can't set the prev bucket block while the block for an allocated block");
        }
        this->prev_bucket_block = prev;
        if (prev) {
            prev->next_bucket_block = this;
        }
    }

    MallocMetadata *getNextBucketBlock() {
        if (not this->isFree()) {
            throw StillAllocatedException("Can't get the next bucket block of an allocated block");
        }
        return this->next_bucket_block;
    }

    MallocMetadata *getPrevBucketBlock() {
        if (not this->isFree()) {
            throw StillAllocatedException("Can't get the prev bucket block of an allocated block");
        }
        return this->prev_bucket_block;
    }

    void *getBucketPtr() {
        if (not this->isFree()) {
            throw StillAllocatedException("Can't get the bucket of an allocated block");
        }
        return this->bucket_ptr;
    }

    void setBucketPtr(void *bucket) {
        if (not this->isFree()) {
            throw StillAllocatedException("Can't set the bucket of an allocated block");
        }
        this->bucket_ptr = bucket;
    }

    void destroy(){
        if (this->is_free){
            num_of_free_bytes -= this->size;
            num_of_free_blocks--;
        } else {
            num_of_allocated_bytes -= this->size;
            num_of_allocated_blocks--;
        }
        MallocMetadata *next = this->getNextInHeap();
        if (next) {
            next->prev = this->prev;
        }
        this->size = 0;
        this->prev_bucket_block = this->prev = this->next_bucket_block = nullptr;
    }
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

void MallocMetadata::setFree() {
    num_of_allocated_blocks--;
    num_of_free_blocks++;
    num_of_free_bytes += this->getSize();
    num_of_allocated_bytes -= this->getSize();
    this->is_free = true;
    this->mergeWithAdjacent();
    // The state of `this` is undefined after using mergeWithAdjacent
}

MallocMetadata *MallocMetadata::getNextInHeap() {
    if (this == page_block_tail) {
        return nullptr;
    }
    return (MallocMetadata *) (((char *) this) + sizeof(MallocMetadata) + this->size);
}

void Bucket::addBlock(MallocMetadata *block) {
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

    // TODO: Maybe add validation for the correct block size. Probably not really required

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
                MallocMetadata *leftover = (MallocMetadata *) ((char *) (curr + 1) + size);
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
    MallocMetadata *adjacent = nullptr;
    // Try merge with adjacent free blocks
    if ((adjacent = this->getNextInHeap())) {
        if (adjacent->isFree()) {
            if (adjacent == page_block_tail){
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
            if (this == page_block_tail){
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
    Bucket *bucket = (Bucket *) this->getBucketPtr();
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
        // TODO: Big allocation
        MallocMetadata *p = (MallocMetadata *) mmap(nullptr,
                                          size + sizeof(MallocMetadata),
                                          PROT_EXEC | PROT_READ | PROT_WRITE,
                                          MAP_ANONYMOUS | MAP_PRIVATE,
                                          -1,
                                          0);
        p->init(size, nullptr, false);
        return p + 1;
    }

    // TODO: Reimplement this to fit the new buckets thingy
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
        num_of_allocated_bytes -= curr->getSize();
        num_of_allocated_blocks--;
        munmap(curr, curr->getSize() + sizeof(MallocMetadata));
        return;
    }
    curr->setFree();
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
    if (size >= KB * NUM_OF_BUCKETS) {
        auto *p = (MallocMetadata *) mmap(nullptr,
                                          size + sizeof(MallocMetadata),
                                          PROT_READ | PROT_WRITE,
                                          MAP_ANONYMOUS,
                                          -1,
                                          0);
        p->init(size, nullptr, false);
        memcpy(p + 1, oldp, curr->getSize());
        munmap(curr, curr->getSize());
        return p + 1;
    }
    if (curr->getSize() >= size) {
        return oldp;// TODO: split excess if there's a "long" leftover
    }
    MallocMetadata *prev = curr->getPrevInHeap();
    MallocMetadata *next = curr->getNextInHeap();
    if (prev and prev->isFree() and prev->getSize() + curr->getSize() >= size) {
        //merge with only the prev block
        prev->removeSelfFromBucketChain();
        prev->setAllocated();
        prev->setSize(prev->getSize() + curr->getSize() + sizeof(MallocMetadata));
        memmove(prev + 1, oldp, curr->getSize());
        curr->destroy();
        if (prev->getSize() >= MIN_SPLIT_BLOCK_SIZE_BYTES + size) {
            size_t leftover_size = prev->getSize() - sizeof(MallocMetadata) - size;
            prev->setSize(size);
            // Split the block and add the leftover to the current bucket
            MallocMetadata *leftover = (MallocMetadata *) ((char *) (prev + 1) + size);
            leftover->init(leftover_size, prev, true);
            buckets[SIZE_TO_BUCKET(leftover_size)].addBlock(leftover);
        }
        return prev;
    } else if (next and next->isFree() and next->getSize() + curr->getSize() >= size) {
        //merge with only the next block
        next->removeSelfFromBucketChain();
        curr->setSize(curr->getSize() + next->getSize() + sizeof(MallocMetadata));
        next->destroy();
        if (curr->getSize() >= MIN_SPLIT_BLOCK_SIZE_BYTES + size) {
            size_t leftover_size = curr->getSize() - sizeof(MallocMetadata) - size;
            curr->setSize(size);
            // Split the block and add the leftover to the current bucket
            MallocMetadata *leftover = (MallocMetadata *) ((char *) (curr + 1) + size);
            leftover->init(leftover_size, curr, true);
            buckets[SIZE_TO_BUCKET(leftover_size)].addBlock(leftover);
        }
        return curr;
    } else if (curr != page_block_tail and prev and next->isFree() and prev->isFree()
               and prev->getSize() + next->getSize() + curr->getSize() >= size) {
        //merge with the next and prev block
        next->removeSelfFromBucketChain();
        prev->removeSelfFromBucketChain();
        prev->setAllocated();
        prev->setSize(prev->getSize() + curr->getSize() + next->getSize() + 2 * sizeof(MallocMetadata));
        memmove(prev + 1, oldp, curr->getSize());
        curr->destroy();
        next->destroy();
        if (prev->getSize() >= MIN_SPLIT_BLOCK_SIZE_BYTES + size) {
            size_t leftover_size = prev->getSize() - sizeof(MallocMetadata) - size;
            prev->setSize(size);
            // Split the block and add the leftover to the current bucket
            MallocMetadata *leftover = (MallocMetadata *) ((char *) (prev + 1) + size);
            leftover->init(leftover_size, prev, true);
            buckets[SIZE_TO_BUCKET(leftover_size)].addBlock(leftover);
        }
        return prev;
    } else if (curr == page_block_tail) {
        sbrk(size - curr->getSize());
        curr->setSize(size);
    } else {
        //allocate an entirely new block, and free the old block
        void *new_block = smalloc(size);
        if (!new_block) {
            return nullptr;
        }
        memmove(new_block, oldp, ((MallocMetadata *) oldp-1)->getSize());
        sfree(oldp);
        return new_block;
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

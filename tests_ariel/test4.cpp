//
// Created by ariel on 25/06/2021.
//

#include <cstdlib>
#include <unistd.h>
#include <sstream>
#include <sys/wait.h>
#include <chrono>
#include "printMemoryList4.h"
#include "malloc_3.h"
#include "colors.h"

using namespace std;

/////////////////////////////////////////////////////

#define MAX_ALLOC 23

#define TEST(X) string X (void *array[MAX_ALLOC])

static int test_ind = 0;

//if you see garbage when printing remove this line or comment it
#define USE_COLORS

// Copy your type here
// don't change anything from the one in malloc_3.c !!not even the order of args!!!
class MallocMetadata {
    struct {
        unsigned int is_free: 1;
        unsigned int is_mmap: 1;
    } flags;
    size_t size;
    MallocMetadata *prev_in_heap;
    MallocMetadata *next_bucket_block;
    MallocMetadata *prev_bucket_block;
    void *bucket_ptr;
    void *user_indicator;

    void mergeWithAdjacent();

public:
    void init(size_t new_size, MallocMetadata *new_prev, bool new_is_free, bool is_mmap);

    size_t getSize() const;

    void setSize(size_t new_size);

    bool isFree() const;

    void setFree();

    void setAllocated();

    MallocMetadata *getPrevInHeap();

    MallocMetadata *getNextInHeap();

    void removeSelfFromBucketChain();

    void setNextBucketBlock(MallocMetadata *next);

    void setPrevBucketBlock(MallocMetadata *new_prev);

    MallocMetadata *getNextBucketBlock();

    MallocMetadata *getPrevBucketBlock();

    void *getBucketPtr();

    void setBucketPtr(void *bucket);

    bool isMmap() const;

    void destroy();

    void *getUserDataAddress();
};




///////////////////////////////////////////////////


typedef std::string (*TestFunc)(void *[MAX_ALLOC]);

void *memory_start_addr;

int max_test_name_len;

int size_of_metadata;

int default_block_size;

int size_for_mmap = 128 * 1024 + 1;

stats current_stats;

std::string default_block;
std::string block_of_2;
std::string block_of_3;

#define DO_MALLOC(x) do{ \
        if(!(x)){                \
           std::cerr << "Failed to allocate at line: "<< __LINE__ << ". command: "<< std::endl << #x << std::endl; \
           exit(1) ;\
        }                \
}while(0)

void checkStats(size_t bytes_mmap, int blocks_mmap, int line_number);

///////////////test functions/////////////////////


TEST(testInit) {
    std::string expected = "|F:8||U:8|";
    if (sizeof(MallocMetadata) - sizeof(void *) != _size_meta_data()) {
        std::cout << "You didn't copy the metadata as is Or a bug in  _size_meta_data()" << std::endl;
    }
    printMemory<MallocMetadata>(memory_start_addr, true);
    checkStats(0, 0, __LINE__);
    DO_MALLOC(array[0] = smalloc(8));
    checkStats(0, 0, __LINE__);
    printMemory<MallocMetadata>(memory_start_addr, true);
    return expected;
}

TEST(testAlignSanity) {
    std::string expected = "|U:8|U:8||F:" + to_string(16 + size_of_metadata) + "||U:64|";
    DO_MALLOC(array[0] = smalloc(5));
    if (((size_t) (array[0])) % 8 != 0) {
        cout << "memory not aligned: " << (size_t) array[0];
    }
    checkStats(0, 0, __LINE__);
    DO_MALLOC(array[1] = smalloc(3));
    checkStats(0, 0, __LINE__);
    printMemory<MallocMetadata>(memory_start_addr, true);
    sfree(array[0]);
    sfree(array[1]);
    checkStats(0, 0, __LINE__);
    printMemory<MallocMetadata>(memory_start_addr, true);
    DO_MALLOC(array[0] = smalloc(1));
    printMemory<MallocMetadata>(memory_start_addr, true);
    return expected;
}

TEST(testAlignSplit) {
    const int split_size = 10 * 1024;
    const int unaligned_big = 501;
    const int alignment_padding_for_big = (8 - (unaligned_big % 8) % 8);
    string expected = "|U:" + to_string(split_size) + "|";
    DO_MALLOC(array[0] = smalloc(split_size));
    printMemory<MallocMetadata>(memory_start_addr, true);
    checkStats(0, 0, __LINE__);
    sfree(array[0]);
    printMemory<MallocMetadata>(memory_start_addr, true);
    expected += "|F:" + to_string(split_size) + "|";
    checkStats(0, 0, __LINE__);
    DO_MALLOC(array[0] = smalloc(unaligned_big));
    printMemory<MallocMetadata>(memory_start_addr, true);
    expected += "|U:" + to_string(unaligned_big + alignment_padding_for_big) +
                "|F:" + to_string(split_size - unaligned_big - alignment_padding_for_big - size_of_metadata) +
                "|";
    checkStats(0, 0, __LINE__);
    expected += "|U:" + to_string(unaligned_big + alignment_padding_for_big);
    int count = 20;
    for (int i = 0; i < count; i++) {
        expected += "|U:" + to_string(8);
        DO_MALLOC(array[1 + i] = smalloc((i % 8) + 1));
        if ((size_t) array[i + 1] % 8 != 0) {
            cout << "allocation misaligned: " << to_string((size_t) array[i + 1]);
        }
    }
    expected += "|F:" + to_string(split_size - unaligned_big - alignment_padding_for_big - count * 8 - count * size_of_metadata - size_of_metadata) + "|";
    printMemory<MallocMetadata>(memory_start_addr, true);
    checkStats(0, 0, __LINE__);
    return expected;
}

TEST(testAlignMmap) {
    const int misaligned_mmap_size = size_for_mmap % 8 == 0 ? size_for_mmap + 3 : size_for_mmap;
    const int padding = 8 - misaligned_mmap_size % 8;
    string expected = "";
    DO_MALLOC(array[0] = smalloc(misaligned_mmap_size));
    checkStats(misaligned_mmap_size + padding, 1, __LINE__);
    if ((size_t) array[0] % 8 != 0) {
        cout << "mmap allocation misaligned: " << to_string((size_t) array[0]);
    }
    DO_MALLOC(array[1] = smalloc(misaligned_mmap_size));
    checkStats((misaligned_mmap_size + padding) * 2, 2, __LINE__);
    if ((size_t) array[1] % 8 != 0) {
        cout << "mmap allocation misaligned: " << to_string((size_t) array[1]);
    }

    return expected;
}

TEST(testAlignCalloc) {
    string expected = "|U:16||U:16|U:24|U:8|U:24|";
    DO_MALLOC(array[0] = scalloc(3, 3));
    checkStats(0, 0, __LINE__);
    printMemory<MallocMetadata>(memory_start_addr, true);
    DO_MALLOC(array[1] = scalloc(3, 7));
    checkStats(0, 0, __LINE__);
    DO_MALLOC(array[1] = smalloc(7));
    checkStats(0, 0, __LINE__);
    DO_MALLOC(array[1] = scalloc(3, 7));
    checkStats(0, 0, __LINE__);
    printMemory<MallocMetadata>(memory_start_addr, true);
    return expected;

}

TEST(testAlignRealloc) {
    const int init_size = 400;
    const int eventual_free = init_size - 32 - 64 - size_of_metadata * 3 - 8;
    string expected = "|U:" + to_string(init_size) + "|"
                      + "|F:" + to_string(init_size) + "|"
                      + "|U:8|U:32|F:" + to_string(init_size - 40 - size_of_metadata * 2) + "|"
                      + "|F:8|U:32|U:64|F:" + to_string(eventual_free) + "|"
                      + "|F:8|U:32|U:64|U:160|"
                      + "|F:8|U:32|U:64|F:160|";

    {
        DO_MALLOC(array[0] = smalloc(400 - 7));
        checkStats(0, 0, __LINE__);
        printMemory<MallocMetadata>(memory_start_addr, true);
    }
    {
        sfree(array[0]);
        checkStats(0, 0, __LINE__);
        printMemory<MallocMetadata>(memory_start_addr, true);
    }
    {
        DO_MALLOC(array[0] = smalloc(7));
        checkStats(0, 0, __LINE__);
        DO_MALLOC(array[1] = smalloc(28)); // Aligned to 32
        checkStats(0, 0, __LINE__);
        printMemory<MallocMetadata>(memory_start_addr, true);
    }
    {
        DO_MALLOC(array[1] = srealloc(array[0], 60)); // Aligned to 64
        checkStats(0, 0, __LINE__);
        printMemory<MallocMetadata>(memory_start_addr, true);
    }
    {
        DO_MALLOC(array[2] = smalloc(eventual_free + 1));
        checkStats(0, 0, __LINE__);
        printMemory<MallocMetadata>(memory_start_addr, true);
    }
    {
        sfree(array[2]);
        checkStats(0, 0, __LINE__);
        printMemory<MallocMetadata>(memory_start_addr, true);
    }

    return expected;
}

/////////////////////////////////////////////////////

#ifdef USE_COLORS
#define PRED(x) FRED(x)
#define PGRN(x) FGRN(x)
#endif
#ifndef USE_COLORS
#define PRED(x) x
#define PGRN(x) x
#endif

void *getMemoryStart() {
    void *first = smalloc(1);
    if (!first) { return nullptr; }
    void *start = (char *) first - _size_meta_data();
    sfree(first);
    return start;
}

void printTestName(std::string &name) {
    std::cout << name;
    for (int i = (int) name.length(); i < max_test_name_len; ++i) {
        std::cout << " ";
    }
}

bool checkFunc(std::string (*func)(void *[MAX_ALLOC]), void *array[MAX_ALLOC], std::string &test_name) {
    std::cout.flush();
    std::stringstream buffer;
    // Redirect std::cout to buffer
    std::streambuf *prevcoutbuf = std::cout.rdbuf(buffer.rdbuf());

    // BEGIN: Code being tested
    std::string expected = func(array);
    // END:   Code being tested

    // Use the string value of buffer to compare against expected output
    std::string text = buffer.str();
    int result = text.compare(expected);
    // Restore original buffer before exiting
    std::cout.rdbuf(prevcoutbuf);
    if (result != 0) {
        printTestName(test_name);
        std::cout << ": " << PRED("FAIL") << std::endl;
        std::cout << "expected: '" << expected << "\'" << std::endl;
        std::cout << "recived:  '" << text << "\'" << std::endl;
        std::cout.flush();
        return false;
    } else {
        printTestName(test_name);
        std::cout << ": " << PGRN("PASS") << std::endl;
    }
    std::cout.flush();
    return true;
}
/////////////////////////////////////////////////////

TestFunc functions[] = {testInit, testAlignSanity, testAlignSplit, testAlignMmap, testAlignCalloc, testAlignRealloc, NULL};
std::string function_names[] = {"testInit", "testAlignSanity", "testAlignSplit", "testAlignMmap", "testAlignCalloc", "testAlignRealloc"};

void checkStats(size_t bytes_mmap, int blocks_mmap, int line_number) {
    updateStats<MallocMetadata>(memory_start_addr, current_stats, bytes_mmap, blocks_mmap);
    if (_num_allocated_blocks() != current_stats.num_allocated_blocks) {
        std::cout << "num_allocated_blocks is not accurate at line: " << line_number << std::endl;
        std::cout << "Expected: " << current_stats.num_allocated_blocks << std::endl;
        std::cout << "Recived:  " << _num_allocated_blocks() << std::endl;
    }
    if (_num_allocated_bytes() != current_stats.num_allocated_bytes) {
        std::cout << "num_allocated_bytes is not accurate at line: " << line_number << std::endl;
        std::cout << "Expected: " << current_stats.num_allocated_bytes << std::endl;
        std::cout << "Recived:  " << _num_allocated_bytes() << std::endl;
    }
    if (_num_meta_data_bytes() != current_stats.num_meta_data_bytes) {
        std::cout << "num_meta_data_bytes is not accurate at line: " << line_number << std::endl;
        std::cout << "Expected: " << current_stats.num_meta_data_bytes << std::endl;
        std::cout << "Recived:  " << _num_meta_data_bytes() << std::endl;
    }
    if (_num_free_blocks() != current_stats.num_free_blocks) {
        std::cout << "num_free_blocks is not accurate at line: " << line_number << std::endl;
        std::cout << "Expected: " << current_stats.num_free_blocks << std::endl;
        std::cout << "Recived:  " << _num_free_blocks() << std::endl;
    }
    if (_num_free_bytes() != current_stats.num_free_bytes) {
        std::cout << "num_free_bytes is not accurate at line: " << line_number << std::endl;
        std::cout << "Expected: " << current_stats.num_free_bytes << std::endl;
        std::cout << "Recived:  " << _num_free_bytes() << std::endl;
    }
}


void initTests() {
    resetStats(current_stats);
    DO_MALLOC(memory_start_addr = getMemoryStart());
    checkStats(0, 0, __LINE__);
    size_of_metadata = _size_meta_data();
    default_block_size = 4 * (size_of_metadata + (4 * 128)); // big enough to split a lot
    if (default_block_size * 3 + size_of_metadata * 2 >= 128 * 1024) {
        default_block_size /= 2;
        std::cerr << "Metadata may be to big for some of the tests" << std::endl;
    }

    default_block = std::to_string(default_block_size);
    block_of_2 = std::to_string(default_block_size * 2 + size_of_metadata);
    block_of_3 = std::to_string(default_block_size * 3 + size_of_metadata * 2);

    max_test_name_len = function_names[0].length();
    for (int i = 0; functions[i] != NULL; ++i) {
        if (max_test_name_len < (int) function_names[i].length()) {
            max_test_name_len = function_names[i].length();
        }
    }
    max_test_name_len++;
}

void printInitFail() {
    std::cerr << "Init Failed , ignore all other tests" << std::endl;
    std::cerr << "The test get the start of the memory list using an allocation of size 1 and free it right after" << std::endl;
    std::cerr << "If this failed you didnt increase it to allocate the next one (Wilderness)" << std::endl;
    std::cerr.flush();
}

void printDebugInfo() {
    std::cout << "Info For Debuging:" << std::endl << "Your Metadata size is: " << size_of_metadata << std::endl;
    std::cout << "Default block size for tests is: " << default_block_size << std::endl;
    std::cout << "Default 2 block after merge size is: " << default_block_size * 2 + size_of_metadata << std::endl;
    std::cout << "Default 3 block after merge size is: " << default_block_size * 3 + size_of_metadata * 2 << std::endl << std::endl;
}

void printStartRunningTests() {
    std::cout << "RUNNING TESTS: (MALLOC PART 3)" << std::endl;
    std::string header = "TEST NAME";
    std::string line = "";
    int offset = (max_test_name_len - (int) header.length()) / 2;
    header.insert(0, offset, ' ');
    line.insert(0, max_test_name_len + 9, '-');
    std::cout << line << std::endl;
    printTestName(header);
    std::cout << " STATUS" << std::endl;
    std::cout << line << std::endl;
}

void printEnd() {
    std::string line = "";
    line.insert(0, max_test_name_len + 9, '-');
    std::cout << line << std::endl;
}


int main(int argc, char **argv) {
    void *allocations[MAX_ALLOC];
    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::duration;
    using std::chrono::milliseconds;
    int wait_status;
    bool ans;
    initTests();
    if (argc >= 2) {
        test_ind = atoi(argv[1]);
    } else {
        printDebugInfo();
        std::cout.flush();
        printStartRunningTests();
    }


    auto t1 = high_resolution_clock::now();

    if (functions[test_ind] == NULL) {
        exit(0);
    }

    ans = checkFunc(functions[test_ind], allocations, function_names[test_ind]);
    if (test_ind == 0 && !ans) {
        printInitFail();
    } else {
        printTestName(function_names[test_ind]);
    }
    std::cout << std::endl;
    execl(argv[0], argv[0], to_string(test_ind + 1).c_str(), NULL);
    printEnd();
    auto t2 = high_resolution_clock::now();
    duration<double, std::milli> ms_double = t2 - t1;
    std::cout << "Total Run Time: " << ms_double.count() << "ms";


    return 0;
}
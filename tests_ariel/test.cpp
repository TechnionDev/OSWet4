//
// Created by ariel on 25/06/2021.
//

#include <cstdlib>
#include <sstream>
#include <sys/wait.h>
#include <chrono>
#include "printMemoryList.h"
#include "malloc_3.h"
#include "colors.h"

/////////////////////////////////////////////////////

#define MAX_ALLOC 23

//if you see garbage when printing remove this line or comment it
#define USE_COLORS

// Copy your type here
// don't change anything from the one in malloc_3.c !!not even the order of args!!!
typedef struct MallocMetadata3 {
	size_t size;
	bool is_free;
	bool on_heap;
	MallocMetadata3 *next;
	MallocMetadata3 *prev;
} Metadata3;


///////////////////////////////////////////////////


#define NUM_FUNC 18

typedef std::string (*TestFunc)(void *[MAX_ALLOC]);

void *memory_start_addr;

int max_test_name_len;

int size_of_metadata;

int default_block_size;

int size_for_mmap = 128 * 1024 +1;

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
void freeAll(void *array[MAX_ALLOC]) {
	for (int i = 0 ; i < MAX_ALLOC ; ++i) {
		sfree(array[i]);
	}
}


std::string allocNoFree(void *array[MAX_ALLOC]) {
	std::string expected = "|U:100|U:100000|U:10|";
	if (MAX_ALLOC < 4) {
		std::cout << "Test Wont work with MAX_ALLOC < 4";
		return expected;
	}

	DO_MALLOC(array[0] = smalloc(100));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[1] = smalloc(100000));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[2] = smalloc(10));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[3] = smalloc(11e6));
	checkStats(11e6, 1, __LINE__);
	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}

std::string allocandFree(void *array[MAX_ALLOC]) {

	std::string expected = "";
	for (int i = 0 ; i < MAX_ALLOC ; ++i) {
		if (i % 5 == 0) {
			expected += "|F:" + default_block;
		} else {
			expected += "|U:" + default_block;
		}
	}
	expected += "|";

	if (MAX_ALLOC < 6) {
		std::cout << "Test Wont work with MAX_ALLOC < 6";
		return expected;
	}

	for (int i = 0 ; i < MAX_ALLOC ; ++i) {
		DO_MALLOC(array[i] = smalloc(default_block_size));
	}
	checkStats(0, 0, __LINE__);
	for (int i = 0 ; i < MAX_ALLOC ; i += 5) {
		sfree(array[i]);
	}
	checkStats(0, 0, __LINE__);

	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}

std::string allocandFreeMerge(void *array[MAX_ALLOC]) {

	std::string expected = "";
	int j = 1;
	for (; j + 8 < MAX_ALLOC ; j += 10) {
		expected += "|U:" + default_block;
		expected += "|F:" + block_of_2;
		expected += "|U:" + default_block;
		expected += "|F:" + block_of_2;
		expected += "|U:" + default_block;
		expected += "|F:" + block_of_3;
	}
	for (; j - 1 < MAX_ALLOC ; ++j) {
		expected += "|U:" + default_block;
	}
	expected += "|";

	if (MAX_ALLOC < 10) {
		std::cout << "Test Wont work with MAX_ALLOC < 10";
		return expected;
	}

	for (int i = 0 ; i < MAX_ALLOC ; ++i) {
		DO_MALLOC(array[i] = smalloc(default_block_size));
	}
	checkStats(0, 0, __LINE__);
	for (int i = 1 ; i + 8 < MAX_ALLOC ; i += 10) {
		checkStats(0, 0, __LINE__);
		sfree(array[i]);
		checkStats(0, 0, __LINE__);
		sfree(array[i + 1]);
		checkStats(0, 0, __LINE__);
		//keep +2 allocated

		//check if order matter
		sfree(array[i + 4]);
		checkStats(0, 0, __LINE__);
		sfree(array[i + 3]);
		checkStats(0, 0, __LINE__);
		//keep +5 allocated

		//check if merge 3
		sfree(array[i + 6]);
		checkStats(0, 0, __LINE__);
		sfree(array[i + 8]);
		checkStats(0, 0, __LINE__);
		sfree(array[i + 7]);
		checkStats(0, 0, __LINE__);
	}
	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}

/**
 * test if the info was copied to the right place
 * @param array
 * @return
 */
std::string testRealloc(void *array[MAX_ALLOC]) {
	std::string expected = "|U:" + block_of_2;
	for (int i = 2 ; i < MAX_ALLOC ; ++i) {
		expected += "|U:" + default_block;
	}
	expected += "|";

	if (MAX_ALLOC < 2) {
		std::cout << "Test Wont work with MAX_ALLOC < 2";
		return expected;
	}

	for (int i = 0 ; i < MAX_ALLOC ; ++i) {
		DO_MALLOC(array[i] = smalloc(default_block_size));
	}
	checkStats(0, 0, __LINE__);

	for (int i = 0 ; i < default_block_size ; ++i) {
		((char *) array[0])[i] = 'b';
		((char *) array[1])[i] = 'a';
	}
	sfree(array[0]);
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[1] = srealloc(array[1], default_block_size * 2));
	checkStats(0, 0, __LINE__);

	for (int i = 0 ; i < default_block_size ; ++i) {
		if (((char *) array[0])[i] != 'a') {
			std::cout << "realloc didnt copy the char b to index " << i << std::endl;
			break;
		}
	}

	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}

/**
 * test if we used the small option for realloc
 * and if the info was copied to the right place
 * @param array
 * @return
 */
std::string testRealloc2(void *array[MAX_ALLOC]) {
	std::string expected = "";
	for (int i = 0 ; i < MAX_ALLOC ; ++i) {
		if (i == 3 || i == 5 || i == 7 || i == 9) {
			expected += "|F:" + default_block;
		} else {
			expected += "|U:" + default_block;
		}
	}
	expected += "|F:" + std::to_string(default_block_size * 3) + "|U:" + std::to_string(default_block_size * 4) + "|U:" +
				std::to_string(default_block_size * 2) + "|";

	if (MAX_ALLOC < 10) {
		std::cout << "Test Wont work with MAX_ALLOC < 10";
		return expected;
	}

	for (int i = 0 ; i < MAX_ALLOC ; ++i) {
		DO_MALLOC(array[i] = smalloc(default_block_size));
	}
	checkStats(0, 0, __LINE__);
	sfree(array[5]);
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[5] = smalloc(default_block_size * 3));
	checkStats(0, 0, __LINE__);
	sfree(array[9]);
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[9] = smalloc(default_block_size * 4));
	checkStats(0, 0, __LINE__);
	sfree(array[7]);
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[7] = smalloc(default_block_size * 2));
	checkStats(0, 0, __LINE__);

	for (int i = 0 ; i < default_block_size ; ++i) {
		((char *) array[3])[i] = 'b';
		((char *) array[5])[i] = 'a';
		((char *) array[7])[i] = 'd';
	}
	sfree(array[5]);
	checkStats(0, 0, __LINE__);
	sfree(array[7]);
	checkStats(0, 0, __LINE__);

	DO_MALLOC(array[3] = srealloc(array[3], default_block_size * 2));
	checkStats(0, 0, __LINE__);
	for (int i = 0 ; i < default_block_size ; ++i) {
		if (((char *) array[7])[i] != 'b') {
			std::cout << "realloc didnt copy the char b to index " << i << std::endl;
			break;
		}
	}

	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}

std::string testWild(void *array[MAX_ALLOC]) {
	std::string expected = "";
	for (int i = 0 ; i < MAX_ALLOC - 10 ; ++i) {
		expected += "|U:" + default_block;
	}
	expected += "|U:" + std::to_string(default_block_size * 3);
	expected += "|U:" + std::to_string(default_block_size * 2) + "|";

	if (MAX_ALLOC < 10) {
		std::cout << "Test Wont work with MAX_ALLOC < 10";
		return expected;
	}

	for (int i = 0 ; i < MAX_ALLOC - 9 ; ++i) {
		DO_MALLOC(array[i] = smalloc(default_block_size));
	}
	checkStats(0, 0, __LINE__);
	sfree(array[MAX_ALLOC - 10]);
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[MAX_ALLOC - 10] = smalloc(default_block_size * 3));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[MAX_ALLOC - 9] = smalloc(default_block_size));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[MAX_ALLOC - 9] = srealloc(array[MAX_ALLOC - 9], default_block_size * 2));
	checkStats(0, 0, __LINE__);

	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}

std::string testSplitAndMerge(void *array[MAX_ALLOC]) {
	int small_part_of_block = default_block_size / 3;
	std::string expected =
			"|U:" + std::to_string(small_part_of_block) + "|F:" + std::to_string(default_block_size - small_part_of_block - size_of_metadata);
	expected += "|U:" + default_block;
	expected += "|U:" + default_block;
	expected += "|U:" + std::to_string(small_part_of_block + default_block_size);
	expected += "|U:" + std::to_string(small_part_of_block + default_block_size);
	expected += "|F:" + std::to_string(default_block_size - 2 * small_part_of_block);
	expected += "|F:" + default_block;
	expected += "|U:" + default_block;
	expected += "|U:" + default_block;
	expected += "|U:" + std::to_string(small_part_of_block + default_block_size);
	expected += "|F:" + std::to_string(default_block_size - small_part_of_block);
	expected += "|U:" + default_block;
	expected += "|U:" + std::to_string(small_part_of_block + default_block_size);
	expected += "|F:" + std::to_string(default_block_size - small_part_of_block);
	for (int i = 14 ; i < MAX_ALLOC ; ++i) {
		expected += "|U:" + default_block;
	}
	expected += "|";;


	if (MAX_ALLOC < 14) {
		std::cout << "Test Wont work with MAX_ALLOC < 14";
		return expected;
	}

	for (int i = 0 ; i < MAX_ALLOC ; ++i) {
		DO_MALLOC(array[i] = smalloc(default_block_size));
	}
	checkStats(0, 0, __LINE__);
	sfree(array[0]);
	checkStats(0, 0, __LINE__);
	DO_MALLOC(smalloc(default_block_size / 3));
	checkStats(0, 0, __LINE__);
	sfree(array[3]);
	checkStats(0, 0, __LINE__);
	sfree(array[6]);
	checkStats(0, 0, __LINE__);
	DO_MALLOC(srealloc(array[4], default_block_size + default_block_size / 3));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(srealloc(array[5], default_block_size + default_block_size / 3));
	checkStats(0, 0, __LINE__);
	sfree(array[10]);
	checkStats(0, 0, __LINE__);
	DO_MALLOC(srealloc(array[9], default_block_size + default_block_size / 3));
	checkStats(0, 0, __LINE__);

	sfree(array[12]);
	checkStats(0, 0, __LINE__);
	DO_MALLOC(srealloc(array[13], default_block_size + default_block_size / 3));
	checkStats(0, 0, __LINE__);

	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}

std::string testCalloc(void *array[MAX_ALLOC]) {
	std::string expected = "|U:100|U:20|U:100|";
	if (MAX_ALLOC < 4) {
		std::cout << "Test Wont work with MAX_ALLOC < 4";
		return expected;
	}

	DO_MALLOC(array[0] = scalloc(1, 100));
	checkStats(0, 0, __LINE__);
	for (int i = 0 ; i < 100 ; ++i) {
		if (((char *) array[0])[i] != 0) {
			std::cout << "array[0] isn't all 0 (first bad index:" << i << ")\n";
			break;
		}
	}
	DO_MALLOC(array[1] = scalloc(2, 10));
	checkStats(0, 0, __LINE__);
	for (int i = 0 ; i < 20 ; ++i) {
		if (((char *) array[1])[i] != 0) {
			std::cout << "array[1] isn't all 0 (first bad index:" << i << ")\n";
			break;
		}
	}
	DO_MALLOC(array[2] = scalloc(10, 10));
	checkStats(0, 0, __LINE__);
	for (int i = 0 ; i < 100 ; ++i) {
		if (((char *) array[2])[i] != 0) {
			std::cout << "array[2] isn't all 0 (first bad index:" << i << ")\n";
			break;
		}
	}
	DO_MALLOC(array[3] = scalloc(100, 10000));
	checkStats(10000 * 100, 1, __LINE__);
	for (int i = 0 ; i < 1000000 ; ++i) {
		if (((char *) array[3])[i] != 0) {
			std::cout << "array[3] isn't all 0 (first bad index:" << i << ")\n";
			break;
		}
	}


	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}

std::string testFreeAllAndMerge(void *array[MAX_ALLOC]) {
	if (MAX_ALLOC > 10e8) {
		std::cout << "Test Wont work with MAX_ALLOC > 10e8";
		return "";
	}
	int allsize = 0;
	for (int i = 0 ; i < MAX_ALLOC ; ++i) {
		DO_MALLOC(array[i] = smalloc(i + 1));
		allsize += i + 1 + (int) _size_meta_data();
	}
	checkStats(0, 0, __LINE__);
	allsize -= (int) _size_meta_data();
	std::string expected = "|F:" + std::to_string(allsize) + "|";

	for (int i = 0 ; i < MAX_ALLOC ; ++i) {
		sfree(array[i]);
	}
	checkStats(0, 0, __LINE__);

	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}

std::string testInit(void *array[MAX_ALLOC]) {
	std::string expected = "|F:1||U:2|";
	if (sizeof(Metadata3) != _size_meta_data()) {
		std::cout << "You didn't copy the metadata as is Or a bug in  _size_meta_data()" << std::endl;
	}
	printMemory<Metadata3>(memory_start_addr, true);
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[0] = smalloc(2));
	checkStats(0, 0, __LINE__);
	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}

std::string testBadArgs(void *array[MAX_ALLOC]) {
	std::string expected = "|U:1|";
	size_t options[3] = {static_cast<size_t>(-1), 0, static_cast<size_t>(10e8 + 1)};
	DO_MALLOC(array[9] = smalloc(1));
	checkStats(0, 0, __LINE__);
	for (size_t option : options) {
		array[0] = smalloc(option);  //need to fail
		checkStats(0, 0, __LINE__);
		array[1] = scalloc(option, 1); //need to fail
		checkStats(0, 0, __LINE__);
		array[2] = scalloc(1, option); //need to fail
		checkStats(0, 0, __LINE__);
		array[3] = srealloc(array[9], option); //need to fail
		checkStats(0, 0, __LINE__);
		if (array[0] || array[1] || array[2] || array[3]) {
			std::cout << "missed edge case: " << std::to_string(option) << std::endl;
		}
	}
	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}

std::string testReallocMMap(void *array[MAX_ALLOC]) {
	std::string expected = "|F:1|";
	for (int i = 0 ; i < MAX_ALLOC ; ++i) {
		DO_MALLOC(array[i] = smalloc(size_for_mmap));
	}
	checkStats(MAX_ALLOC * size_for_mmap, MAX_ALLOC, __LINE__);

	for (int i = 0 ; i < size_for_mmap ; ++i) {
		((char *) array[0])[i] = 'b';
		((char *) array[1])[i] = 'a';
	}
	sfree(array[0]);
	checkStats((MAX_ALLOC - 1) * size_for_mmap, MAX_ALLOC - 1, __LINE__);
	DO_MALLOC(array[1] = srealloc(array[1], size_for_mmap * 2));
	checkStats((MAX_ALLOC) * size_for_mmap, MAX_ALLOC - 1, __LINE__);
	for (int i = 0 ; i < size_for_mmap ; ++i) {
		if (((char *) array[1])[i] != 'a') {
			std::cout << "realloc didnt copy the char a to index " << i << std::endl;
			break;
		}
	}

	DO_MALLOC(array[1] = srealloc(array[1], size_for_mmap)); //test  decreasing
	checkStats((MAX_ALLOC - 1) * size_for_mmap, MAX_ALLOC - 1, __LINE__);

	for (int i = 0 ; i < size_for_mmap ; ++i) {
		if (((char *) array[1])[i] != 'a') {
			std::cout << "realloc didnt copy the char a to index " << i << std::endl;
			break;
		}
	}
	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}


std::string testReallocDec(void *array[MAX_ALLOC]) {
	std::string expected = "|F:" + std::to_string(default_block_size * 2);
	expected += "|U:" + std::to_string(default_block_size);
	expected += "|F:" + std::to_string(default_block_size - size_of_metadata);
	for (int i = 2 ; i < MAX_ALLOC ; ++i) {
		expected += "|U:" + std::to_string(default_block_size * 2);
	}
	expected += "|";


	for (int i = 0 ; i < MAX_ALLOC ; ++i) {
		DO_MALLOC(array[i] = smalloc(default_block_size * 2));
	}
	checkStats(0, 0, __LINE__);

	for (int i = 0 ; i < default_block_size * 2 ; ++i) {
		((char *) array[0])[i] = 'b';
		((char *) array[1])[i] = 'a';
	}
	sfree(array[0]);
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[1] = srealloc(array[1], default_block_size));
	checkStats(0, 0, __LINE__);

	for (int i = 0 ; i < default_block_size ; ++i) {
		if (((char *) array[1])[i] != 'a') {
			std::cout << "realloc didnt copy the char a to index " << i << std::endl;
			break;
		}
	}

	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}


std::string testReallocDecOverwrite(void *array[MAX_ALLOC]) {
	std::string expected = "|U:" + std::to_string(default_block_size * 3 +size_of_metadata);
	expected += "|";
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[0] = smalloc(default_block_size));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[1] = smalloc(default_block_size * 2));
	checkStats(0, 0, __LINE__);

	DO_MALLOC(array[2] = smalloc(size_for_mmap));
	checkStats(size_for_mmap, 1, __LINE__);

	for (int i = 0 ; i < default_block_size * 2 ; ++i) {
		((char *) array[1])[i] = (char) i;
	}
	for (int i = 0 ; i < default_block_size ; ++i) {
		((char *) array[0])[i] = (char) i;
	}
	for (int i = 0 ; i < size_for_mmap ; ++i) {
		((char *) array[2])[i] = (char) i;
	}

	checkStats(size_for_mmap, 1, __LINE__);
	sfree(array[0]);

	checkStats(size_for_mmap, 1, __LINE__);
	DO_MALLOC(array[1] = srealloc(array[1], default_block_size * 3));
	checkStats(size_for_mmap, 1, __LINE__);
	DO_MALLOC(array[2] = srealloc(array[2], size_for_mmap * 3));
	checkStats(size_for_mmap*3, 1, __LINE__);

	for (int i = 0 ; i < default_block_size * 2 ; ++i) {
		if (((char *) array[1])[i] != (char) i) {
			std::cout << "realloc didnt copy the index " << i << std::endl;
			break;
		}
	}
	for (int i = 0 ; i < size_for_mmap ; ++i) {
		if (((char *) array[2])[i] != (char) i) {
			std::cout << "realloc didnt copy the index " << i << std::endl;
			break;
		}
	}

	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}


/**
 * don't merge recursivly
 * don't merge on split
 * @param array
 * @return
 */
std::string testNoRecMerge(void *array[MAX_ALLOC]) {
	std::string expected = "" ;
	std::string start = "|U:" + std::to_string(default_block_size / 3);
	start += "|F:" + std::to_string(default_block_size - (default_block_size / 3) - size_of_metadata);

	expected += start;
	expected += "|F:" + std::to_string(default_block_size);
	expected += "|U:" + std::to_string(default_block_size);
	expected += "|";
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[0] = smalloc(default_block_size));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[1] = smalloc(default_block_size));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[2] = smalloc(default_block_size));

	checkStats(0, 0, __LINE__);
	sfree(array[1]);
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[0] = srealloc(array[0], default_block_size / 3));
	checkStats(0, 0, __LINE__);
	printMemory<Metadata3>(memory_start_addr, true);
	expected += start;
	expected += "|F:" + std::to_string(default_block_size *2 + size_of_metadata);
	expected += "|";

	sfree(array[2]);
	checkStats(0, 0, __LINE__);

	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}

std::string testReallocWildLikePiazza(void *array[MAX_ALLOC]) {
	std::string expected = "" ;
	expected += "|F:" + std::to_string(default_block_size*4);
	expected += "|U:" + std::to_string(default_block_size);
	expected += "|F:" + std::to_string(default_block_size);
	expected += "|U:" + std::to_string( default_block_size *3) + "|";
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[0] = smalloc(default_block_size*4));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[1] = smalloc(default_block_size));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[2] = smalloc(default_block_size));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[3] = smalloc(default_block_size));

	checkStats(0, 0, __LINE__);
	sfree(array[0]);
	checkStats(0, 0, __LINE__);
	sfree(array[2]);
	checkStats(0, 0, __LINE__);
	for (int i = 0 ; i < default_block_size ; ++i) {
		((char *) array[3])[i] = (char) i;
	}
	DO_MALLOC(array[3] = srealloc(array[3], default_block_size *3));
	checkStats(0, 0, __LINE__);
	for (int i = 0 ; i < default_block_size ; ++i) {
		if (((char *) array[3])[i] != (char) i) {
			std::cout << "realloc didnt copy the index " << i << std::endl;
			break;
		}
	}

	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}


std::string testReallocWilderness2(void *array[MAX_ALLOC]) {
	std::string expected = "|U:" + std::to_string(default_block_size);
	expected += "|F:" + std::to_string(default_block_size * 3);
	expected += "|U:" + std::to_string(default_block_size * 2);
	expected += "|U:" + std::to_string(default_block_size * 2 + _size_meta_data()) + "|";
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[0] = smalloc(default_block_size));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[1] = smalloc(default_block_size * 3));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[2] = smalloc(default_block_size * 2));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[3] = smalloc(default_block_size));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[4] = smalloc(default_block_size));
	checkStats(0, 0, __LINE__);

	for (int i = 0 ; i < default_block_size ; ++i) {
		((char *) array[4])[i] = (char) i;
	}

	checkStats(0, 0, __LINE__);

	sfree(array[3]);
	checkStats(0, 0, __LINE__);

	sfree(array[1]);
	checkStats(0, 0, __LINE__);

	DO_MALLOC(array[4] = srealloc(array[4], default_block_size * 2));
	checkStats(0, 0, __LINE__);
	if (array[4] != array[3]) {
		std::cout << "srealloc on wilderness didn't merge with adjacent block" << std::endl;
	}
	printMemory<Metadata3>(memory_start_addr, true);

	for (int i = 0 ; i < default_block_size ; ++i) {
		if (((char *) array[3])[i] != (char) i) {
			std::cout << "realloc didnt copy the char a to index " << i << std::endl;
			break;
		}
	}
	void *last4ThItem = array[4];
	DO_MALLOC(array[4] = srealloc(array[4], default_block_size * 3));
	checkStats(0, 0, __LINE__);
	if (array[4] != last4ThItem) {
		std::cout << "srealloc on wilderness didn't increse it though no merge is available" << std::endl;
	}
	for (int i = 0 ; i < default_block_size ; ++i) {
		if (((char *) array[4])[i] != (char) i) {
			std::cout << "realloc didnt copy the char a to index " << i << std::endl;
			break;
		}
	}
	expected += "|U:" + std::to_string(default_block_size);
	expected += "|F:" + std::to_string(default_block_size * 3);
	expected += "|U:" + std::to_string(default_block_size * 2);
	expected += "|U:" + std::to_string(default_block_size * 3) + "|";
	printMemory<Metadata3>(memory_start_addr, true);
	return expected;
}

std::string mergeVariations(void *array[MAX_ALLOC]) {
	std::string expected = "|U:" + std::to_string(default_block_size);
	expected += "|U:" + std::to_string(default_block_size);
	expected += "|U:" + std::to_string(default_block_size * 2 + _size_meta_data());
	expected += "|U:" + std::to_string(default_block_size) + "|";
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[0] = smalloc(default_block_size));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[1] = smalloc(default_block_size));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[2] = smalloc(default_block_size ));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[3] = smalloc(default_block_size));
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[4] = smalloc(default_block_size));
	checkStats(0, 0, __LINE__);

	for (int i = 0 ; i < default_block_size ; ++i) {
		((char *) array[2])[i] = (char) i;
	}

	checkStats(0, 0, __LINE__);

	sfree(array[3]);
	char firstByteInFreedBlock = ((char *)array[3])[0];
	checkStats(0, 0, __LINE__);
	void *last2ndItem = array[2];
	DO_MALLOC(array[2] = srealloc(array[2], default_block_size * 2));
	checkStats(0, 0, __LINE__);
	if (array[2] != last2ndItem) {
		std::cout << "srealloc not on right place" << std::endl;
	}
	//if (((char *)array[3])[0] != firstByteInFreedBlock) {
	//	std::cout << "content in freed right block changed" << std::endl;
	//}
	printMemory<Metadata3>(memory_start_addr, true);

	for (int i = 0 ; i < default_block_size ; ++i) {
		if (((char *) array[2])[i] != (char) i) {
			std::cout << "realloc didnt keep the char a to index " << i << std::endl;
			break;
		}
	}

	sfree(array[1]);
	checkStats(0, 0, __LINE__);
	sfree(array[4]);
	checkStats(0, 0, __LINE__);
	DO_MALLOC(array[2] = srealloc(array[2], default_block_size * 4));
	checkStats(0, 0, __LINE__);
	if (array[2] != array[1]) {
		std::cout << "srealloc not on right place" << std::endl;
	}
	for (int i = 0 ; i < default_block_size ; ++i) {
		if (((char *) array[1])[i] != (char) i) {
			std::cout << "realloc didnt copy the char a to index " << i << std::endl;
			break;
		}
	}
	expected += "|U:" + std::to_string(default_block_size);
	expected += "|U:" + std::to_string(default_block_size * 4 + 3 * _size_meta_data()) + "|";
	printMemory<Metadata3>(memory_start_addr, true);
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
	for (int i = (int) name.length() ; i < max_test_name_len ; ++i) {
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

TestFunc functions[NUM_FUNC] = {testInit, allocNoFree, allocandFree, testFreeAllAndMerge, allocandFreeMerge, testRealloc, testRealloc2, testWild,
								testSplitAndMerge, testCalloc, testBadArgs, testReallocMMap, testReallocDec , testReallocDecOverwrite , testNoRecMerge , testReallocWildLikePiazza , testReallocWilderness2 ,mergeVariations};
std::string function_names[NUM_FUNC] = {"testInit", "allocNoFree", "allocandFree", "testFreeAllAndMerge", "allocandFreeMerge", "testRealloc",
										"testRealloc2",
										"testWild",
										"testSplitAndMerge", "testCalloc", "testBadArgs", "testReallocMMap", "testReallocDec" ,"testReallocDecOverwrite" ,"testNoRecMerge" ,"testReallocWildLikePiazza" , "testReallocWilderness2" ,"mergeVariations"};

void checkStats(size_t bytes_mmap, int blocks_mmap, int line_number) {
	updateStats<Metadata3>(memory_start_addr, current_stats, bytes_mmap, blocks_mmap);
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
	for (int i = 0 ; i < NUM_FUNC ; ++i) {
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


int main() {
	void *allocations[MAX_ALLOC];
	using std::chrono::high_resolution_clock;
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using std::chrono::milliseconds;
	int wait_status;
	bool ans;


	initTests();
	printDebugInfo();
	std::cout.flush();
	printStartRunningTests();

	auto t1 = high_resolution_clock::now();

	for (int i = 0 ; i < NUM_FUNC ; ++i) {
		pid_t pid = fork();
		if (pid == 0) {
			ans = checkFunc(functions[i], allocations, function_names[i]);
			if (i == 0 && !ans) {
				printInitFail();
			}
			exit(0);
		} else {
			wait(&wait_status);
			if (!WIFEXITED(wait_status) || (WEXITSTATUS(wait_status)) != 0) {
				printTestName(function_names[i]);
				std::cout << ": " << PRED("FAIL + CRASHED");
				if (WCOREDUMP (wait_status)) {
					std::cout << PRED(" (Core Dumped)");
				} else if (WIFSIGNALED(wait_status)) {
					std::cout << " Exit Signal:" << WTERMSIG(wait_status) << std::endl;
				}
				std::cout << std::endl;
			}
		}
	}
	printEnd();
	auto t2 = high_resolution_clock::now();
	duration<double, std::milli> ms_double = t2 - t1;
	std::cout << "Total Run Time: " << ms_double.count() << "ms";


	return 0;
}
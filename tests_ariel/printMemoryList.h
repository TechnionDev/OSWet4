//
// Created by ariel on 25/06/2021.
//

#ifndef OS_WET4_PRINTMEMORYLIST_H
#define OS_WET4_PRINTMEMORYLIST_H

//void printMemory1(void* start);
//void printMemory2(void* start);
//void printMemory3(void* start);
//void printMemory4(void* start);

#include <iostream>

typedef struct stats_t {
	size_t num_free_blocks = 0;
	size_t num_free_bytes = 0;
	size_t num_allocated_blocks = 0;
	size_t num_allocated_bytes = 0;
	size_t num_meta_data_bytes = 0;
} stats;


template<class T>
void printMemory(void *start, bool onlyList) {
	T *current = (T *) start;
	size_t size = 0;
	int blocks = 0;
	if (!onlyList) {
		std::cout << "Printing Memory List\n";
	}
	while (current) {
		if (current->is_free) {
			std::cout << "|F:" << current->size;
		} else {
			std::cout << "|U:" << current->size;
		}
		size += current->size;
		blocks++;
		current = current->next;
	}
	std::cout << "|";
	if (!onlyList) {
		std::cout << std::endl << "Memory Info:\nNumber Of Blocks: " << blocks << "\nTotal Size (without Metadata): " << size << std::endl;
		std::cout << "Size of Metadata: " << sizeof(T) << std::endl;
	}
}

void resetStats(stats &current_stats) {
	current_stats.num_allocated_blocks = 0;
	current_stats.num_allocated_bytes = 0;
	current_stats.num_free_blocks = 0;
	current_stats.num_free_bytes = 0;
	current_stats.num_meta_data_bytes = 0;
}


template<class T>
void updateStats(void *start, stats &current_stats, size_t bytes_mmap, int blocks_mmap) {
	resetStats(current_stats);
	T *current = (T *) start;
	while (current) {
		current_stats.num_meta_data_bytes += sizeof(T);
		current_stats.num_allocated_bytes += current->size;
		current_stats.num_allocated_blocks++;
		if (current->is_free) {
			current_stats.num_free_blocks++;
			current_stats.num_free_bytes += current->size;
		}
		current = current->next;
	}
	current_stats.num_meta_data_bytes += sizeof(T) * blocks_mmap;
	current_stats.num_allocated_bytes += bytes_mmap;
	current_stats.num_allocated_blocks += blocks_mmap;
}


#endif //OS_WET4_PRINTMEMORYLIST_H

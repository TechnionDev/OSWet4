#include <iostream>
#include <unistd.h>
#define MAX_SIZE 100000000
void *smalloc(size_t size) {
    if (size == 0 || size > MAX_SIZE) {
        return nullptr;
    }
    void *old_brk = sbrk(0);
    if (old_brk == (void *) -1) {
        return nullptr;
    }
    sbrk(size);

    return old_brk;
}

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}

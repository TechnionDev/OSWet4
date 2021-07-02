#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include "os_malloc.h"

#define KB 1024

int main(int argc, char const *argv[])
{

    void* p1 = smalloc(100);
    std::cout << p1 << std::endl;
    void* p2 = smalloc(129*KB);
    std::cout << p2 << std::endl;
    return 0;
}

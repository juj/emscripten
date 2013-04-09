#include <stdio.h>

int counter = 0;

extern "C"
{

void __attribute__((noinline)) increment_counter()
{
    ++counter;
}

}

int main()
{
    for(int i = 0; i < 1000000; ++i)
        increment_counter();
}

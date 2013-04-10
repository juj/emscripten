#include <stdio.h>
#include <emscripten.h>

int counter = 0;

extern "C"
{

void __attribute__((noinline)) increment_counter()
{
    ++counter;
}

}

int __attribute__((noinline)) complexFunc()
{
    return (int)(emscripten_get_now() * 0.00000001f) + 1000000;
}

int main()
{
    int i;
    for(i = 0; i < complexFunc(); ++i)
        increment_counter();
    printf("complexFunc: %d\n", complexFunc());
    printf("%d\n", i);
}

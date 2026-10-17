#include <stdio.h>
#include <stdlib.h>


int main(int argc, char** argv)
{
    if(argc != 2)
    {
        printf("USAGE: %s <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE* file = fopen(argv[1], "r");
    if(!file)
    {
        printf("Failed to open %s for reading!\n", argv[1]);
        return EXIT_FAILURE;
    }

    // TODO: ваш код

    return EXIT_SUCCESS;
}

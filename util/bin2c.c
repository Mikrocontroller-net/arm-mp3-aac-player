/*
 * bin2c: Converts a binary file into a C array
 * Bart Trzynadlowski
 * March 27, 2001; June 11, 2001
 */

#include <stdio.h>

int main(int argc, char **argv)
{
    FILE            *fp;
    long            size, i;
    unsigned char   data;

    if (argc <= 1)
    {
        puts("bin2c by Bart Trzynadlowski: Binary->C Converter");
        puts("Usage:\tbin2c <file>");
        exit(0);
    }

    if ((fp = fopen(argv[1], "rb")) == NULL)
    {
        fprintf(stderr, "bin2c: Unable to open file: %s\n", argv[1]);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    puts("data_type\tarray_name[] =");
    puts("{");

    i = 0;
    while (size--)
    {
        fread(&data, sizeof(unsigned char), 1, fp);

        if (!i)
            printf("\t");
        if (size == 0)
            printf("0x%02X\n};\n", data);
        else
            printf("0x%02X,", data);
        i++;
        if (i == 14)
        {
            printf("\n");
            i = 0;
        }
    }

    fclose(fp);
    return 0;
}
            


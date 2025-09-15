#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main()
{
    FILE *fp = fopen("random_1GB.txt", "w");
    if (!fp)
    {
        perror("fopen");
        return 1;
    }

    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    size_t charset_size = sizeof(charset) - 1;     // exclude null terminator
    const size_t target_size = 1024 * 1024 * 1024; // 1 GB

    srand((unsigned)time(NULL));

    for (size_t i = 0; i < target_size; i++)
    {
        char c = charset[rand() % charset_size];
        fputc(c, fp);
    }

    fclose(fp);
    printf("Generated random_1GB.txt with random alphabets.\n");
    return 0;
}

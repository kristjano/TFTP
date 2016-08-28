#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Two arguments required: tftpd [port] [dir]\n");
        exit(1);
    }
    
    int port = atoi(argv[1]);
    char *dir = argv[2];

    printf("port: %i\n dir: %s\n", port, dir);
    return 0;
}

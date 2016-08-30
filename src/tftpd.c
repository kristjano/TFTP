#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

int RRQ();
int WRQ();
int DATA();
int ACK();
int ERROR();

int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: tftpd [port] [dir]\n");
        exit(1);
    }

    int port = atoi(argv[1]); // port number from program arguments
    char *dir = argv[2]; // directory path from program arguments
    int sockfd, dirfd;
    struct sockaddr_in server, client;
    char message[516];

    // Open directory to share
    // TODO

    /* Create and bind a UDP socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;

    /* Network functions need arguments in network byte order instead
     * of host byte order. The macros htonl, htons convert the
     * values.
     */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    bind(sockfd, (struct sockaddr *) &server, (socklen_t) sizeof(server));

    for (;;) {
        /* Receive up to one byte less than declared, because it will
         * be NUL-terminated later.
         */
        socklen_t len = (socklen_t) sizeof(client);
        ssize_t n = recvfrom(sockfd, message, sizeof(message) - 1,
                             0, (struct sockaddr *) &client, &len);

        message[n] = '\0';
        
        int opcode = message[1];
        char filename[512];
        
        switch (opcode)
        {
            case 1:
                RRQ();
                break;
            case 2:
                WRQ();
                break;
            case 3:
                RRQ();
                break;
            case 4:
                RRQ();
                break;
            case 5:
                RRQ();
                break;
            default:
                ERROR();
        }
        
        sprintf(filename, "%s", &message[2]);
        
        fprintf(stdout, "Received:\n%s\n", filename);
        
        fflush(stdout);

        /* convert message to upper case. */
        for (int i = 0; i < n; ++i) {
            message[i] = toupper(message[i]);
        }

        sendto(sockfd, message, (size_t) n, 0,
               (struct sockaddr *) &client, len);
    }
    return 0;
}

int RRQ()
{
    printf("RRQ\n");
    return 0;
}

int WRQ()
{
    // do nothing
    printf("WRQ\n");
    return 0;
}

int DATA()
{
    printf("DATA\n");
    return 0;
}

int ACK()
{
    printf("ACK\n");
    return 0;
}

int ERROR()
{
    printf("ERROR\n");
    return 0;
}
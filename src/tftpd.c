#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

int RRQ(char message[]);
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
    int sockfd;
    struct sockaddr_in server, client;
    char message[516];

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
        
        
        switch (opcode)
        {
            case 1:
                RRQ(message);
                break;
            case 2:
                WRQ();
                break;
            case 3:
                DATA();
                break;
            case 4:
                ACK();
                break;
            case 5:
                ERROR();
                break;
            default:
                // Illegal opcode. Do something.
                break;
        }
        
        
        
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

int RRQ(char message[])
{
    //        2 bytes    string   1 byte     string   1 byte
    //        -----------------------------------------------
    // RRQ   |  01   |  Filename  |   0  |    Mode    |   0  |
    //        -----------------------------------------------
    printf("RRQ\n");
    char filename[512], mode[512];
    sprintf(filename, "%s", &message[2]);
    sprintf(mode, "%s", &message[3 + strlen(filename)]);
    fprintf(stdout, "Filename:\n%s\n", filename);
    fprintf(stdout, "Mode:\n%s\n", mode);
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
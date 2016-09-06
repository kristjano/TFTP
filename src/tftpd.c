#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int RRQ(char message[], int sockfd);
int WRQ();
int DATA();
int ACK();
int ERROR();

int data_transfer(int fd, int sockfd);

// struct DATA {
//     unsigned short int op;
//     unsigned short int block;
//     char data[512];
// };

struct sockaddr_in server, client;

int main(int argc, char *argv[])
{
    int port = atoi(argv[1]); // port number from program arguments
    char *dir = argv[2]; // directory path from program arguments
    int sockfd;
    char message[516];

    /* Check usage */
    if (argc < 3) {
        printf("Usage: tftpd [port] [dir]\n");
        exit(1);
    }
    
    /* Change directory */
    if (chdir(dir) == -1)
    {
        fprintf(stderr, "Directory does not exist or is not accessable\n");
        exit(1);
    }

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
                RRQ(message, sockfd);
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
    }
    return 0;
}

int RRQ(char message[], int sockfd)
{
    //        2 bytes    string   1 byte     string   1 byte
    //        -----------------------------------------------
    // RRQ   |  01   |  Filename  |   0  |    Mode    |   0  |
    //        -----------------------------------------------
    printf("RRQ\n");
    
    char filename[512], mode[512];
    int fd;
    //struct DATA datagram;
    sprintf(filename, "%s", &message[2]);
    sprintf(mode, "%s", &message[3 + strlen(filename)]);
    
    fprintf(stdout, "Filename:\n%s\n", filename);
    fprintf(stdout, "Mode:\n%s\n", mode);
    
    /* Open the file to send */
    if ((fd = open(filename, O_RDONLY, S_IRUSR)) == -1) {
        fprintf(stderr, "File does not exist or is not accessable");
        exit(1);
    }

    data_transfer(fd, sockfd);
    
    if (close(fd) == -1) {
        fprintf(stderr, "Error when closing file.");
        exit(1);
    }

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
    // do nothing
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

int data_transfer(int fd, int sockfd)
{
    /* Build the datagram to send */
    // datagram.op = 3;
    // datagram.block = 1;
    // read(fd, datagram.data, sizeof(datagram.data));
    unsigned short int opcode = htons(3);
    unsigned short int blockno = htons(1);
    char data[512];
    char datagram[516];

    memcpy(&datagram[0], &opcode, 2);
    memcpy(&datagram[2], &blockno, 2);
    read(fd, data, sizeof(data));
    snprintf(&datagram[4], sizeof(data), "%s", data);
    // printf("%d", datagram[0]);
    for (int i = 0; i < 516; i++) {
        printf("%x ", datagram[i]);
    }
    
    
    /* Send datagram */
    if (sendto(sockfd, &datagram, sizeof(datagram), 0, 
        (struct sockaddr *) &client, (socklen_t) sizeof(client)) == -1)
            printf("Error sending datagram\n");
    
    return 0;
}
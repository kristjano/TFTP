#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

enum opcodes {RRQ=1, WRQ, DATA, ACK, ERROR};

/* TFTP format packet for sending data. */
typedef struct {
    unsigned short int op;
    unsigned short int block;
    char data[512];
} Data;

int read_request(char message[], int sockfd);
int transfer_data(int fd, int sockfd);
int await_ack(int sockfd, Data *datagram);
int is_path(char filename[]);

/* Global server and client socket address */
struct sockaddr_in server, client;

int main(int argc, char *argv[])
{
    /* Check usage */
    if (argc < 3) {
        printf("Usage: tftpd [port] [dir]\n");
        exit(1);
    }
    
    int port = atoi(argv[1]); // port number from program arguments
    char *dir = argv[2]; // directory path from program arguments
    int sockfd;
    char message[516];
    
    /* Change directory */
    if (chdir(dir) == -1) {
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
            case RRQ:
                read_request(message, sockfd);
                break;
            case WRQ:
                // TODO: send error to client.
                break;
            case ERROR:
                //
                break;
            default:
                // Illegal opcode. Do something.
                break;
        }
    }
    return 0;
}

int read_request(char message[], int sockfd)
{
    //        2 bytes    string   1 byte     string   1 byte
    //        -----------------------------------------------
    // RRQ   |  01   |  Filename  |   0  |    Mode    |   0  |
    //        -----------------------------------------------
    char filename[512], mode[512], ip_address[INET_ADDRSTRLEN];
    int fd;

    inet_ntop(AF_INET, &(client.sin_addr), ip_address, INET_ADDRSTRLEN);
    sprintf(filename, "%s", &message[2]);
    sprintf(mode, "%s", &message[3 + strlen(filename)]);
    
    if (is_path(filename) == -1) {
        fprintf(stderr, "Error: Filename contains path.\n");
        // TODO: send error to client.
        return -1;
    }
    
    fprintf(stdout, "file \"%s\" requested from %s:%hu\n", filename, 
            ip_address, client.sin_port);
    
    if ((strncasecmp("netascii", mode, 8) == 0) || 
        (strncasecmp("octet", mode, 5) == 0))  {
        /* Open the file to send */
        if ((fd = open(filename, O_RDONLY)) == -1) {
            fprintf(stderr, "File does not exist or is not accessable");
            exit(1);
        }
    } else {
        fprintf(stderr, "Illegal mode");
        return -1;
    }

    transfer_data(fd, sockfd);
    
    if (close(fd) == -1) {
        fprintf(stderr, "Error when closing file.");
        exit(1);
    }

    return 0;
}

int transfer_data(int fd, int sockfd)
{
    /* Build the datagram to send */
    //        2 bytes    2 bytes       n bytes
    //        ---------------------------------
    // DATA  | 03    |   Block #  |    Data    |
    //        ---------------------------------
    Data datagram;
    unsigned short int block = 1;
    
    /* OP code and block number need to be in network byte order */
    datagram.op = htons(3);
    
    for (;;) {
        datagram.block = htons(block);
        ssize_t data_size = read(fd, datagram.data, sizeof(datagram.data));
        
        /* Send datagram */
        if (sendto(sockfd, &datagram, data_size + 4, 0, 
            (struct sockaddr *) &client, (socklen_t) sizeof(client)) == -1) {
            printf("Error sending datagram.\n");
            return -1;
        }
       
        if (await_ack(sockfd, &datagram) == -1) {
            printf("Error received from client.\n");
            return -1;
        }

        /* If data_size is less than 512 bytes then we have sent 
         * the whole file. */
        if (data_size < 512) break;
        else ++block;
    }
    
    return 0;
}

int await_ack(int sockfd, Data *datagram)
{
    //        2 bytes    2 bytes
    //        -------------------
    // ACK   | 04    |   Block #  |
    //        --------------------
    char message[516];
    for (;;) {
        socklen_t len = (socklen_t) sizeof(client);
        ssize_t n = recvfrom(sockfd, message, sizeof(message) - 1,
                             0, (struct sockaddr *) &client, &len);
        
        message[n] = '\0';
        
        int opcode = message[1];
        
        switch (opcode)
        {
            case ACK:
                // TODO: Validate block number. 
                return 0;
                break;
            case ERROR:
                //
                break;
            default:
                // Illegal opcode. Do something.
                break;
        }
    }
}

int is_path(char filename[])
{
    int len = strlen(filename);
    for (int i = 0; i < len; i++) {
        if (filename[i] == '/') {
            return -1;
        }
    }
    return 0;
}

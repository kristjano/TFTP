#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define WAIT 100 // Timout in 5 ms.
#define MAX_RESEND 5 // Maximum resends.

enum opcodes {RRQ=1, WRQ, DATA, ACK, ERROR};

/* TFTP format packet for sending data. */
typedef struct {
    unsigned short int op;
    unsigned short int block;
    char data[512];
} Data;

/* TFTP format packet for sending error. */
typedef struct {
    unsigned short int op;
    unsigned short int code;
    char message[512];
} Error;

int read_request(char message[], int sockfd);
int transfer_data(int fd, int sockfd);
int await_ack(int sockfd, int tries, Data *payload);
int send_error(int sockfd, short int code, char message[]);
int is_path(char filename[]);
unsigned short extract_littleend16(char *buf);

/* Global server and client socket address */
struct sockaddr_in server, client;

int main(int argc, char *argv[])
{
    /* Check usage */
    if (argc < 3) {
        fprintf(stdout, "Usage: tftpd [port] [dir]\n");
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
        
        switch (opcode) {
            case RRQ:
                read_request(message, sockfd);
                break;
            case WRQ:
                // This is an illegal TFTP opretion on this server.
                send_error(sockfd, 4, "Illegal TFTP operation.");
                break;
            case ERROR:
                // Do nothing.
                break;
            default:
                // Illegal opcode. Do nothing.
                break;
        }
    }
    return 0;
}

/* Analyse RRQ. */
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
    
    /* Filename cannot be path */
    if (is_path(filename) == -1) {
        fprintf(stderr, "Error: Filename contains path.\n");
        send_error(sockfd, 2, "Access violation.");
        return -1;
    }
    
    /* Open the file to send */
    if ((fd = open(filename, O_RDONLY)) == -1) {
        fprintf(stderr, "File does not exist or is not accessable.\n");
        send_error(sockfd, 1, "File not found.");
        return -1;
    }
    
    /* Check if legal mode is set. */
    if (!((strncasecmp("netascii", mode, 8) == 0) || 
        (strncasecmp("octet", mode, 5) == 0)))  {
            fprintf(stderr, "Illegal mode\n");
            send_error(sockfd, 4, "Illegal TFTP operation.");
            return -1;
    }
    
    fprintf(stdout, "file \"%s\" requested from %s:%hu\n", filename, 
            ip_address, client.sin_port);
    fflush(stdout);

    transfer_data(fd, sockfd);
    
    if (close(fd) == -1) {
        fprintf(stderr, "Error when closing file.\n");
        return -1;
    }

    return 0;
}

/* Read and transfer file. */
int transfer_data(int fd, int sockfd)
{
    /* Build the datagram to send */
    //        2 bytes    2 bytes       n bytes
    //        ---------------------------------
    // DATA  | 03    |   Block #  |    Data    |
    //        ---------------------------------
    Data payload;
    ssize_t data_size;
    unsigned short int block;
    int resend;
    
    /* OP code and block number need to be in network byte order */
    payload.op = htons(3);
    
    for (block = 1;;++block) {
        payload.block = htons(block);
        data_size = read(fd, payload.data, sizeof(payload.data));
        resend = 0;

        /* Loop for resending packet. */
        for (;;) {
            /* Send datagram */
            if (sendto(sockfd, &payload, data_size + 4, 0, 
                (struct sockaddr *) &client, (socklen_t) sizeof(client)) == -1) {
                fprintf(stderr, "Error sending datagram.\n");
                return -1;
            }

            resend = await_ack(sockfd, resend, &payload);

            if (resend == -1) {
                /* Error from client. Or timeout to often. */
                fprintf(stderr, "Data transfer aborted.\n");
                return -1;
            } else if (resend == 0) {
                /* Packet received */
                break;
            } else {
                /* Timeout. Resend. */
            }
        }
        
       /* If data_size is less than 512 bytes then we have sent 
        * the whole file. */
       if (data_size < 512) break;
    }
    
    return 0;
}

/* Wait for ACK packet from client. */
int await_ack(int sockfd, int tries, Data *payload)
{
    //        2 bytes    2 bytes
    //        -------------------
    // ACK   | 04    |   Block #  |
    //        --------------------
    if (tries > MAX_RESEND) {
        /* Packet already been sent MAX_RESEND times. */
        return -1;
    }
    
    char message[516];
    unsigned short int rec_block;
    
    /* Timeout settings */
    fd_set sock_set;
    struct timeval  timeout;
    int message_received;
    
    FD_ZERO(&sock_set);
    FD_SET(sockfd, &sock_set);
    timeout.tv_sec = 0;
    timeout.tv_usec = WAIT;
    
    for (;;) {
        message_received = select(sockfd + 1, &sock_set, NULL, NULL, &timeout);
    
        if (message_received == -1) {
            fprintf(stderr, "Error when waiting for message.\n");
            return -1;
        }
        
        if (message_received == 0) {
            /* Timeout */
            return tries + 1;
        }

        socklen_t len = (socklen_t) sizeof(client);
        ssize_t n = recvfrom(sockfd, message, sizeof(message) - 1,
                             0, (struct sockaddr *) &client, &len);
        
        message[n] = '\0';
        
        int opcode = message[1];
        
        switch (opcode) {
            case ACK:
                if ((rec_block = extract_littleend16(&message[2])) != payload->block) {
                    /* Received most likely douplicated ACK from previous
                     * transmission. Wait for next ACK.*/
                    fprintf(stderr, "Wrong block number in ACK.\n");
                    fprintf(stderr, "Expected: %hu\n", ntohs(payload->block));
                    fprintf(stderr, "Received: %hu\n", ntohs(rec_block));
                    break;
                }
                return 0;
            case ERROR:
                /* Stop sending. */
                return -1;
            default:
                // Illegal opcode.
                send_error(sockfd, 4, "Illegal TFTP operation.");
                return -1;
        }
    }
}

int send_error(int sockfd, short int code, char message[])
{
    //        2 bytes  2 bytes        string    1 byte
    //        ----------------------------------------
    // ERROR | 05    |  ErrorCode |   ErrMsg   |   0  |
    //        ----------------------------------------
    Error payload;
    int msg_len;
    
    payload.op = htons(5);
    payload.code = htons(code);
    strcpy(payload.message, message);
    
    /* Add null to end of message. */
    msg_len = strlen(payload.message);
    payload.message[msg_len] = '\0';
    
    /* Send datagram */
    if (sendto(sockfd, &payload, msg_len + 5, 0, 
        (struct sockaddr *) &client, (socklen_t) sizeof(client)) == -1) {
        fprintf(stderr, "Error sending datagram.\n");
        return -1;
    }
    return 0;
}

/* Small helper function to see if string contains a path. */
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

/* Extracts 2 bytes from withing string.
 * (borrowed from online: 
 * http://stackoverflow.com/questions/7957381/use-stdcopy-to-copy-two-bytes-from-a-char-array-into-an-unsigned-short
 * )*/
unsigned short extract_littleend16(char *buf)
{
    return (((unsigned short)buf[0]) << 0) |
           (((unsigned short)buf[1]) << 8);
}

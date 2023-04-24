#include "headers.h"

#define TIMEOUT 5.0
#define IMAGE_BUFFER_SIZE 16348
#define RESPONSE_SIZE 32768


/**Function prototypes*/

void send_request(SOCKET s, char* ip_address, char *port, char *buffer, size_t buffer_size);

SOCKET connect_to_server(char *ip_address, char *port);

char* read_image_file(const char* input_filename, off_t* file_size);

int handle_server_response(SOCKET server);


int main(int argc, char *argv[]) {

#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }
#endif

    // Hardcoding server information
    char *ip_address = "127.0.0.1";
    char *port = "8080";
    SOCKET server = connect_to_server(ip_address, port);


    off_t file_size;
    char input[100];

    while (1){
        printf("Enter your command (send/quit): \n");
        scanf("%s", input);
        if (strcmp(input, "send") == 0){
            char filename[100];
            printf("Enter the QR code file name: \n");
            scanf("%s", filename);

            // code to send file to server
            char * image_buffer = read_image_file(filename, &file_size);
            send_request(server, ip_address, port, image_buffer, file_size);
            free(image_buffer);
            handle_server_response(server);

        } else if (strcmp(input, "quit") == 0){
            break;
        } else {
            printf("Invalid command. Please enter 'send' or 'quit'.\n");
        }
    }

finish:

    printf("\nClosing socket...\n");
    CLOSESOCKET(server);

#if defined(_WIN32)
    WSACleanup();
#endif
    printf("Finished.\n");
    return 0;
}



void send_request(SOCKET s, char* ip_address, char *port, char *buffer, size_t buffer_size){
    
    if (send(s, buffer, buffer_size, 0) == -1){
        printf("Error in sending file.\n");
        exit(1);
    }

    printf("Send qr file successfully.\n\n");
}


SOCKET connect_to_server(char *ip_address, char *port){
    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    struct addrinfo *peer_address;
    if (getaddrinfo(ip_address, port, &hints, &peer_address)){
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];

    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
                address_buffer, sizeof(address_buffer),
                service_buffer, sizeof(service_buffer),
                NI_NUMERICHOST);
    printf("%s %s\n", address_buffer, service_buffer);

    printf("Creating socket...\n");
    SOCKET server;
    server = socket(peer_address->ai_family, peer_address->ai_socktype, peer_address->ai_protocol);

    if (!ISVALIDSOCKET(server)){
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    printf("Connecting...\n");
    if(connect(server,
                peer_address->ai_addr,
                peer_address->ai_addrlen)){
                   fprintf(stderr, "connect() failed. (%d)\n", GETSOCKETERRNO());
                   exit(1); 
                }
    freeaddrinfo(peer_address);
    printf("Connected. \n\n");

    return server;
}


char* read_image_file(const char* input_filename, off_t* file_size) {
    int input_fd = open(input_filename, O_RDONLY);
    if (input_fd == -1) {
        printf("Error: could not open input file\n");
        exit(1);
    }

    off_t size = lseek(input_fd, 0, SEEK_END);
    lseek(input_fd, 0, SEEK_SET);
    *file_size = size;

    char* buffer = malloc(sizeof(char) * size);
    ssize_t bytes_read;
    ssize_t total_bytes_read = 0;

    while ((bytes_read = read(input_fd, buffer + total_bytes_read, IMAGE_BUFFER_SIZE)) > 0) {
        total_bytes_read += bytes_read;
    }

    if (bytes_read == -1) {
        printf("Error: could not read from input file\n");
        exit(1);
    }

    close(input_fd);

    if (total_bytes_read != size) {
        printf("Warning: number of bytes read does not match file size\n");
    }

    return buffer;
}


/** function to handle server response */
int handle_server_response(SOCKET server){
    const clock_t start_time = clock();

    

    char response[RESPONSE_SIZE+1];
    char *p = response, *q;
    char *end = response + RESPONSE_SIZE;
    char *body = 0;

    enum {length, chunked, connection};
    int encoding = 0;
    int remaining = 0;

    while(1) {

        if ((clock() - start_time) / CLOCKS_PER_SEC > TIMEOUT) {
            fprintf(stderr, "timeout after %.2f seconds\n", TIMEOUT);
            return 1;
        }

        if (p == end) {
            fprintf(stderr, "out of buffer space\n");
            return 1;
        }

        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(server, &reads);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;

        if (select(server+1, &reads, 0, 0, &timeout) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }

        if (FD_ISSET(server, &reads)) {
            int bytes_received = recv(server, p, end - p, 0);
            if (bytes_received < 1) {
                if (encoding == connection && body) {
                    printf("%.*s", (int)(end - body), body);
                }

                printf("\nConnection closed by peer.\n");
                break;
            }

            printf("%.*s", bytes_received, p);

            p += bytes_received;
            *p = 0;
        } //if FDSET
    } //end while(1)
    return 0;
}


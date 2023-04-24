#include "headers.h"


FILE * log_fp;
#define MAX_REQUEST_SIZE 2047
#define TIME_OUT 5.0 // TODO: To pass as argument
#define MAX_CLIENTS 10 // TODO: To pass as argument


void print_time(FILE *fp, int add_space) {
    time_t t;    
    time(&t);

    char formatted_time[50];
    struct tm *tmp = localtime(&t);
    strftime(formatted_time, 50, "%Y-%m-%d %H:%M:%S", tmp);

    switch (add_space){
        case 1:
            fprintf(fp, "%s", formatted_time);
            break;
        case 2:
            fprintf(fp, "%s ", formatted_time);
            break;
        case 3:
            fprintf(fp, " %s", formatted_time);
            break;
        case 4:
            fprintf(fp, " %s ", formatted_time);
            break;
        case 5:
            fprintf(fp, "%s\t", formatted_time);
            break;
        case 6:
            fprintf(fp, "%s\n", formatted_time);
            break;
    }
    fflush(fp);   
}


void handle_sigint(int sig){
    fprintf(log_fp, "--> Server was stop due to interupt signal at ");
    fflush(log_fp);
    print_time(log_fp, 6);
    exit(0);
}


SOCKET create_socket(const char* host, const char *port, FILE * fp) {
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;

    getaddrinfo(host, port, &hints, &bind_address);

    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family,
            bind_address->ai_socktype, bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen,
                bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    freeaddrinfo(bind_address);

    /**
     * TODO: currently listending: Change from 10 to the MAX USERS
    */

    printf("Listening...\n"); 
    if (listen(socket_listen, 1) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    // server start up time
    fprintf(fp, "\n--> Server started at ");
    fflush(fp);
    print_time(fp, 6);
    return socket_listen;
}



struct client_info {
    socklen_t address_length;
    struct sockaddr_storage address;
    SOCKET socket;
    char request[MAX_REQUEST_SIZE + 1];
    int received; // the number of bytes stored in the request array
    struct client_info *next;
    clock_t last_active; //time when the client first connected
};

static struct client_info *clients = 0;


struct client_info *get_client(SOCKET s) {
    struct client_info *ci = clients;

    while(ci) {
        if (ci->socket == s)
            break;
        ci = ci->next;
    }

    if (ci) return ci;
    struct client_info *n =
        (struct client_info*) calloc(1, sizeof(struct client_info));

    if (!n) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }

    n->address_length = sizeof(n->address);
    n->next = clients;
    clients = n;
    return n;
}

/** Helper function to convert client's IP address to text */

const char *get_client_address(struct client_info *ci) {
    static char address_buffer[100];
    getnameinfo((struct sockaddr*)&ci->address,
            ci->address_length,
            address_buffer, sizeof(address_buffer), 0, 0,
            NI_NUMERICHOST);
    return address_buffer;
}


void drop_client(struct client_info *client) {
    CLOSESOCKET(client->socket);

    struct client_info **p = &clients;

    while(*p) {
        if (*p == client) {
            *p = client->next;
            free(client);

            
            fprintf(log_fp, " |- Dropped client %s at ", get_client_address(client));
            fflush(log_fp);
            print_time(log_fp, 6);

            return;
        }
        p = &(*p)->next;
    }

    fprintf(stderr, "drop_client not found.\n");
    exit(1);
}

fd_set wait_on_clients(SOCKET server) {
    fd_set reads; // a set of sockets
    FD_ZERO(&reads); // initialize the client file descripter set
    FD_SET(server, &reads); // populated the `reads`
    SOCKET max_socket = server;

    struct client_info *ci = clients;

    while(ci) {
        FD_SET(ci->socket, &reads);
        if (ci->socket > max_socket)
            max_socket = ci->socket;
        ci = ci->next;
    }
    // select() returns when one or more of the sockets in `reads`
    if (select(max_socket+1, &reads, 0, 0, 0) < 0) {
        fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    return reads;
}

void send_error(struct client_info *client, char * error_message){
    send(client->socket, error_message, strlen(error_message), 0);
    drop_client(client);
}


/** server resource and then drop client*/

void serve_resource(struct client_info *client, const char *image_content) {
    
    char * output_filename = "qr_recv.png";

    int output_fd = open(output_filename, O_CREAT | O_WRONLY, 0644);
    if (output_fd == -1) {
        printf("Error: could not open output file\n");
        exit(1);
    }

    printf("saving content received from client %s %s\n", get_client_address(client), image_content);

    ssize_t total_bytes_written = 0;
    int bytes_to_write = client->received;

    while (total_bytes_written < bytes_to_write){
        ssize_t bytes_written = write(output_fd, image_content + total_bytes_written, bytes_to_write - total_bytes_written);

        if (bytes_written == -1){
            printf("Error writing to output file.\n");
            exit(1);
        }

        total_bytes_written += bytes_written;
    }

    close(output_fd);

    // run the command and redirect its output to a file
    char command[1024];
    snprintf(command, sizeof(command), "java -cp javase.jar:core.jar com.google.zxing.client.j2se.CommandLineRunner %s", output_filename);
    FILE* fp = popen(command, "r");

    if (fp == NULL){
        printf("Error executing command\n");
        exit(1);
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL){
        send(client->socket, buffer, strlen(buffer), 0);
    }

    pclose(fp);
    
    fprintf(log_fp, " |- Finish serving client %s at ", get_client_address(client));
    fflush(log_fp);
    print_time(log_fp, 6);

    
    drop_client(client);


    int status = remove(output_filename);

    if (status != 0) {
        printf("Unable to delete the file %s\n", output_filename);
        perror("Error");
        exit(1);
    }
}


int main() {

    log_fp = fopen("server.log", "a");

    if (log_fp == NULL){
        printf("Error openning file server.log. \n");
        exit(1);
    }

    /**
     * Error handling for command line arguments
    */

#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }
#endif

    SOCKET server = create_socket("127.0.0.1", "8080", log_fp);
    signal(SIGINT, handle_sigint);

    
    // to handle multiple clients
    int client_fds[MAX_CLIENTS] = {0}; // create an array and initialized elements to be 0
    int max_fd = server;
    int num_clients = 0;

    while(1) {

        fd_set reads;
        reads = wait_on_clients(server);

        if (FD_ISSET(server, &reads)) {

            if (num_clients < MAX_CLIENTS){
                 struct client_info *client = get_client(-1);

                client->socket = accept(server,
                        (struct sockaddr*) &(client->address),
                        &(client->address_length));

                if (!ISVALIDSOCKET(client->socket)) {
                    fprintf(stderr, "accept() failed. (%d)\n",
                            GETSOCKETERRNO());
                    return 1;
                }

                // TODO: set the timer when client is connected and added to the log
                // Must constantly check for the number of connections allowed.
                // if max is reached => may not allowed more connection by disconnecting a client. Check the timeout..
                printf("New connection from %s.\n",
                        get_client_address(client));

                // server.log for client connection
                fprintf(log_fp, " |- New connection from %s at ", get_client_address(client));
                // update clock time
                client->last_active = clock();

                fflush(log_fp);
                print_time(log_fp, 6);

                // increment client
                num_clients++;
            } else {
                fprintf(stderr, "Max clients exceeded. Disconnecting the first client");
                //TODO: dropping the first client
            }
        }


        struct client_info *client = clients;

        // TODO: in this code, instead of while client, => change to while clients < MAX_CLIENTS
        while(client) {
            struct client_info *next = client->next;
             // Handling time_out
            if ((clock() - client->last_active)/CLOCKS_PER_SEC > TIME_OUT){
                send_error(client, "Timeout due to inactivity.\n");
                client = next;
                continue;                    
            }

            if (FD_ISSET(client->socket, &reads)) {

                if (MAX_REQUEST_SIZE == client->received) {
                    send_error(client, "Invalid request.\n");
                    client = next;
                    continue;
                }

                int r = recv(client->socket,
                        client->request + client->received,
                        MAX_REQUEST_SIZE - client->received, 0);
                
                client->last_active = clock();      

                if (r < 1) {
                    printf("Unexpected disconnect from %s.\n",
                            get_client_address(client));
                    
                    fprintf(log_fp, " |- Unexpected disconnect from %s at ", get_client_address(client));
                    fflush(log_fp);
                    print_time(log_fp, 6);    
                    drop_client(client);

                } else {
                    client->received += r;
                    client->request[client->received] = 0;

                    serve_resource(client, client->request);

                    // decrement client
                }
            }

            /**
             * TODO: When to drop clients??
            */
            client = next;
        }
    } //while(1)
    fclose(log_fp);

    printf("\nClosing socket...\n");
    CLOSESOCKET(server);


#if defined(_WIN32)
    WSACleanup();
#endif

    printf("Finished.\n");
    return 0;
}

/***
 * LOG requirement 
 * • Rate limiting (RATE): Without reasonable safeguards, public servers could be attacked. 
 * To prevent this, the server operator should be able to specify the number of QR codes, 
 * specified by “number requests”, that should be allowed in a “number seconds” timeframe. 
 * Requests issued in excess of this rate should be discarded, and the occurrence of this condition logged at the server. 
 * The client should not be disconnected for violating rate limiting. 
 * Instead, they should receive an error message indicating why their command was not processed. 
 * Default: 2 requests per user per 60 seconds.
 * 
 * 
• Maximum number of concurrent users (MAX USERS): Only a specified number of users should be able to 
connect to the QR code server at a given time. Above this threshold, connecting users should receive 
an error message indicating that the server is busy before terminating the connection. 
Such refused connections should be logged at the server. When a user disconnects, a slot should be 
opened for another connection. Default: 3 users.
*/

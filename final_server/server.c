#include "headers.h"


FILE * log_fp;
#define MAX_REQUEST_SIZE 2047

// default options
#define DEFAULT_PORT "2012"
#define DEFAULT_RATE_RQS 2
#define DEFAULT_RATE_TIME 60
#define DEFAULT_MAX_USERS 3
#define DEFAULT_TIME_OUT 80




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


void handle_sigint(){
    fprintf(log_fp, "--> Server was stop due to interupt signal at ");
    fflush(log_fp);
    print_time(log_fp, 6);
    exit(0);
}

void parse_options(int argc, char *argv[], char *port, int *rate_rqs, int * rate_time, int *max_users, int *time_out){
    int opt;
    int option_index;

    struct option long_options[] = {
        {"PORT", required_argument, 0, 'p'},
        {"RATE_MSGS", required_argument, 0, 'r'},
        {"RATE_TIME", required_argument, 0, 't'},
        {"MAX_USERS", required_argument, 0, 'u'},
        {"TIME_OUT", required_argument, 0, 'o'},
        {0, 0, 0, 0}

    };

    while((opt = getopt_long(argc, argv, "p:r:t:u:o:", long_options, &option_index)) != -1){
        switch(opt){
            case 'p':
                port = optarg;
                break;
            case 'r':
                *rate_rqs = atoi(optarg);
                break;
            case 't':
                *rate_time = atoi(optarg);
                break;
            case 'u':
                *max_users = atoi(optarg);
                break;
            case 'o':
                *time_out = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [--PORT port] [-RATE_MSGS rate_msgs] [-RATE_TIME rate_time] [-MAX_USERS max_users] [-TIME_OUT time_out]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
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
    time_t last_request_time; // to monitor the client's rate limit (within a specified duration)
    int request_count; // to monitor client's rate limit (number of requests)
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
    n->request_count = 0; //initialized value
    n->last_request_time = time(NULL);

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

fd_set wait_on_clients(SOCKET server, int time_out) {
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

    // specify a timeout with select
    struct timeval timeout;
    timeout.tv_sec = time_out; //default at 80 secs unless specified by command line option
    timeout.tv_usec = 0;

    if (select(max_socket+1, &reads, 0, 0, &timeout) < 0) {
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

// TODO: to match the response format the professor required.

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

    
    // drop_client(client);

    int status = remove(output_filename);

    if (status != 0) {
        printf("Unable to delete the file %s\n", output_filename);
        perror("Error");
        exit(1);
    }
}


int main(int argc, char *argv[]) {

    log_fp = fopen("server.log", "a");

    if (log_fp == NULL){
        printf("Error openning file server.log. \n");
        exit(1);
    }

    // Initialize the default values;
    char * port = DEFAULT_PORT;
    int rate_rqs = DEFAULT_RATE_RQS;
    int rate_time = DEFAULT_RATE_TIME;
    int max_users = DEFAULT_MAX_USERS;
    int time_out = DEFAULT_TIME_OUT;

    parse_options(argc, argv, port, &rate_rqs, &rate_time, &max_users, &time_out);

    

#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }
#endif

    SOCKET server = create_socket("127.0.0.1", port, log_fp);
    signal(SIGINT, handle_sigint);

    int num_clients = 0;

    while(1) {

        fd_set reads;
        reads = wait_on_clients(server, time_out);

        if (FD_ISSET(server, &reads)) {
            struct client_info *client = get_client(-1);

            client->socket = accept(server,
                    (struct sockaddr*) &(client->address),
                    &(client->address_length));

            if (!ISVALIDSOCKET(client->socket)) {
                fprintf(stderr, "accept() failed. (%d)\n",
                        GETSOCKETERRNO());
                return 1;
            }

            if (num_clients < max_users){
                
                printf("New connection from %s.\n",
                        get_client_address(client));

                fprintf(log_fp, " |- New connection from %s at ", get_client_address(client));

                fflush(log_fp);
                print_time(log_fp, 6);

                num_clients++;

            } else {
                fprintf(log_fp, "Max clients exceeded. Disconnecting incoming clients at ");
                fflush(log_fp);
                print_time(log_fp, 6);

                // send a message to the connecting client to refuse connection
                char * msg = "Server is busy, check back later...";
                send(client->socket, msg, strlen(msg), 0);
                drop_client(client);
            }
        }


        struct client_info *client = clients;

        while(client) {
            struct client_info *next = client->next;

            if (FD_ISSET(client->socket, &reads)) {

                if (MAX_REQUEST_SIZE == client->received) {
                    send_error(client, "Invalid request.\n");
                    client = next;
                    continue;
                }

                int r = recv(client->socket,
                        client->request + client->received,
                        MAX_REQUEST_SIZE - client->received, 0);                     

                if (r < 1) {
                    printf("Unexpected disconnect from %s.\n",
                            get_client_address(client));
                    
                    fprintf(log_fp, " |- Unexpected disconnect from %s at ", get_client_address(client));
                    fflush(log_fp);
                    print_time(log_fp, 6);    
                    drop_client(client);

                    break; // break this while(client) loop                    
                    num_clients--;

                }

                // serve the client
                time_t now = time(NULL);
                double time_diff = difftime(now, client->last_request_time);

                // handling client request rate

                if ((time_diff < rate_time) && (client->request_count > rate_rqs)){
                     printf("Client exceeded request limit: %d", client->socket);
                        char * msg = "You exceeded the request limit, check back in a minute";
                        send(client->socket, msg, strlen(msg), 0);
                } else {
                     // allowing a new request
                    client->request_count = 1;
                    client->last_request_time = now;

                    client->received += r;
                    client->request[client->received] = 0;

                    serve_resource(client, client->request);

                    //clear the request buffer and reset `client->received` to 0 to allow the client to send another request
                    memset(client->request, 0, MAX_REQUEST_SIZE);
                    client->received = 0;


                    // server receive a new request
                    recv(client->socket,
                        client->request + client->received,
                        MAX_REQUEST_SIZE - client->received, 0);

                    serve_resource(client, client->request);
                }
            }
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

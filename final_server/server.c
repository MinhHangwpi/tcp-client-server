#include "headers.h"


FILE * log_fp;
#define MAX_REQUEST_SIZE 8192

// default options
#define DEFAULT_PORT 2012
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

void parse_options(int argc, char *argv[], int *port, int *rate_rqs, int * rate_time, int *max_users, int *time_out){
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
                *port = atoi(optarg);
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


SOCKET create_socket(const char *port, FILE * fp, int max_users) {
    printf("Configuring IP address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;

    getaddrinfo(0, port, &hints, &bind_address);

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

    printf("Listening...\n"); 
    if (listen(socket_listen, max_users) < 0) {
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
    time_t last_active; // to monitor client's interaction for managing timeout
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
    n->last_active = time(NULL);

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
            printf(" |- Dropped client %s at ", get_client_address(client));
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


/** TODO: next*/

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
    
    // printf("saving content received from client %s %s\n", get_client_address(client), image_content);

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

    // run the QR decoder and redirect its output to a file
    char command[1024];
    snprintf(command, sizeof(command), "java -cp javase.jar:core.jar com.google.zxing.client.j2se.CommandLineRunner %s > output.txt 2>&1", output_filename);
    int ret = system(command);
    if (ret == -1){
        printf("Error executing command\n");
        exit(1);
    }

    // Sending the result back to the client
    char buffer[1024];
    size_t response_length = 0;

    // get the length of the response
    FILE* fp = fopen("output.txt", "r+");
    if (fp == NULL) {
        printf("Error opening output file.\n");
        exit(1);
    }
    if (fseek(fp, 0L, SEEK_END) == -1){
        printf("Error with fseek: \n");
        exit(1);
    }

    response_length = ftell(fp);
    
    // Send the length of the response to the client
    
    char response_length_str[256];
    snprintf(response_length_str, sizeof(response_length_str), "%lu", response_length);
    send(client->socket, response_length_str, strlen(response_length_str), 0);

    // rewind file ptr
    rewind(fp);

    ftell(fp);

    // Send the actual response to the client
    while (fgets(buffer, sizeof(buffer), fp) != NULL){
        send(client->socket, buffer, strlen(buffer), 0);
        /* for debugging */
        // printf("%s", buffer);
    }

    pclose(fp);
    
    fprintf(log_fp, " |- Finish serving client %s at ", get_client_address(client));

    printf(" |- Finishing serving client %s\n", get_client_address(client));

    fflush(log_fp);
    print_time(log_fp, 6);

    
    int status = remove(output_filename);

    if (status != 0) {
        printf("Unable to delete the file %s\n", output_filename);
        perror("Error");
        exit(1);
    }
}



void handle_client_timeout(struct client_info *client, int time_out, int * num_clients){
    time_t now = time(NULL);
    double laps_since_last_active = difftime(now, client->last_active);

    if (laps_since_last_active > time_out){
        
        // log event at the server; e.g. client %IP address has been inactive for %s time, terminating due to timeout.
        fprintf(log_fp, " |- Client %s has been inactive for %f seconds\n |- client %s terminated at ", get_client_address(client), laps_since_last_active, get_client_address(client));
        fflush(log_fp);
        print_time(log_fp, 6);

        // send error to client and drop client
        char * msg = "You have been inactive for %f seconds, terminating connection...\n";
        send(client->socket, msg, strlen(msg), 0);

        drop_client(client);
        num_clients--;
    }
}



int main(int argc, char *argv[]) {

    log_fp = fopen("server.log", "a");

    if (log_fp == NULL){
        printf("Error openning file server.log. \n");
        exit(1);
    }

    // Initialize the default values;
    int port = DEFAULT_PORT;
    int rate_rqs = DEFAULT_RATE_RQS;
    int rate_time = DEFAULT_RATE_TIME;
    int max_users = DEFAULT_MAX_USERS;
    int time_out = DEFAULT_TIME_OUT;

    parse_options(argc, argv, &port, &rate_rqs, &rate_time, &max_users, &time_out);
    

#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }
#endif
    // convert int port to string
    char port_str[10];
    sprintf(port_str, "%d", port);

    SOCKET server = create_socket(port_str, log_fp, max_users);
    
    printf("Server configuration %s\n", port_str);
    printf("port: %d\n", port);
    printf("rate_rqs: %d\n", rate_rqs);
    printf("rate_time: %d\n", rate_time);
    printf("max_users: %d\n", max_users);
    printf("time_out: %d\n", time_out);

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
                printf("\nCurrent number of clients: %d\n", num_clients);

                client->last_active = time(NULL);

            } else {
                fprintf(log_fp, " |- Max clients exceeded. Disconnecting incoming client %s at ", get_client_address(client));
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
            // handle client timeout
            handle_client_timeout(client, time_out, &num_clients);

            if (FD_ISSET(client->socket, &reads)) {

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

                if (client->received > MAX_REQUEST_SIZE) {
                    send_error(client, "The request exceeded the maximum size of 8192.\n");
                    printf("Client %s sent a request that is too large, at %d\n", get_client_address(client), client->received);
                    client = next;
                    continue;
                }

                fprintf(log_fp, " |- Received a valid request from client %s at ", get_client_address(client));
                fflush(log_fp);
                print_time(log_fp, 6);
                client->last_active = time(NULL);


                // serve the client
                time_t now = time(NULL);
                double time_diff = difftime(now, client->last_request_time);
                printf("\nBefore new request: %d requests %f ago\n", client->request_count, time_diff);

                // handling client request rate

                if ((time_diff < rate_time) && (client->request_count >= rate_rqs)){
                    printf("Client %s exceeded request limit: %d", get_client_address(client), client->socket);
                    fprintf(log_fp, "Client %s exceeded request limit: %d", get_client_address(client), client->socket);
                    
                    char * msg = "You exceeded the request limit, check back in a minute";
                    send(client->socket, msg, strlen(msg), 0);
                } else {

                    if (time_diff > rate_time){
                        // reset client->request_count
                        client->request_count = 0;
                    }   

                    // allowing a new request and increment request_count
                    client->request_count++;
                    client->last_request_time = now;

                    client->received += r;
                    client->request[client->received] = '\0';

                    if (strcmp(client->request, "shutdown") == 0){
                        fprintf(log_fp, " |- Received a \"shutdown\" request from client %s at ", get_client_address(client));
                        fflush(log_fp);
                        print_time(log_fp, 6);
                        
                        drop_client(client);
                        CLOSESOCKET(server);
                        fprintf(log_fp, "--> Server was stopped after client shutdown request at ");
                        fflush(log_fp);
                        print_time(log_fp, 6);                        
                    }

                    serve_resource(client, client->request);

                    //clear the request buffer and reset `client->received` to 0 to allow the client to send another request
                    memset(client->request, 0, MAX_REQUEST_SIZE);
                    client->received = 0;
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

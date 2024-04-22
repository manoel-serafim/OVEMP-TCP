#include "macro.h"
#include <stdio.h>
#include "libs.h"

pthread_mutex_t exit_mutex = PTHREAD_MUTEX_INITIALIZER;
int should_exit = 0;


//separate into other files latter:
void print_addrinfo(struct addrinfo *info) {
    printf( "%s", CYN);
    while (info != NULL) {
        printf("Address Family: %s\n", (info->ai_family == AF_INET) ? "IPv4" : ((info->ai_family == AF_INET6) ? "IPv6" : "Unknown"));
        printf("Socket Type   : %s\n", (info->ai_socktype == SOCK_STREAM) ? "TCP" : ((info->ai_socktype == SOCK_DGRAM) ? "UDP" : "Unknown"));
        printf("Protocol      : %d\n", info->ai_protocol);

        // Check if there's an address
        if (info->ai_addr != NULL) {
            char addrstr[INET6_ADDRSTRLEN];

            // Assuming IPv4 or IPv6
            if (info->ai_family == AF_INET) {
                struct sockaddr_in *ipv4 = (struct sockaddr_in *)info->ai_addr;
                inet_ntop(AF_INET, &(ipv4->sin_addr), addrstr, INET_ADDRSTRLEN);
                printf("IPv4 Address  : %s\n", addrstr);
            } else if (info->ai_family == AF_INET6) {
                struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)info->ai_addr;
                inet_ntop(AF_INET6, &(ipv6->sin6_addr), addrstr, INET6_ADDRSTRLEN);
                printf("IPv6 Address  : %s\n", addrstr);
            }
        }

        printf( "%s", "\n");

        info = info->ai_next;
    }
    printf( "%s", RESET);
}

void *send_thread(void *arg) {
    int sockfd = *((int *)arg);
    char *send_buffer = NULL;
    size_t buf_size = 0;
    ssize_t line_size;

    printf("%s", YELLOW"[!] READY TO SEND [!]\n"RESET );

    for(;;) {
        pthread_mutex_lock(&exit_mutex);
        if (should_exit) {
            pthread_mutex_unlock(&exit_mutex);
            break;
        }
        printf("\n%s", RED"#> "RESET );
        pthread_mutex_unlock(&exit_mutex);
        

        line_size = getline(&send_buffer, &buf_size, stdin);
        if (line_size < 0) {
            fprintf(stderr, RED"[-] getline error: %d\n"RESET, errno);
            break;
        }

        send(sockfd, send_buffer, line_size, 0);
    }
    
    free(send_buffer);
    return NULL;
}


void *recv_thread(void *arg) {
    int sockfd = *((int *)arg);
    char *recv_buffer = (char *)malloc(BUFFER_SIZE);

    printf("%s", YELLOW"[!] READY TO RECEIVE [!]\n"RESET );

    if (recv_buffer == NULL) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    int nbytes;
    for(;;) {
        
        nbytes = recv(sockfd, recv_buffer, BUFFER_SIZE - 1, 0);
        if (nbytes <= 0) {
            break;
        }
        recv_buffer[nbytes] = '\0';
        printf(CYN"\n#> %s\n"RED"#>", recv_buffer);
        // Check for "bye" message
        pthread_mutex_lock(&exit_mutex);
        if (strcasecmp(recv_buffer, "bye\n") == 0) {
            should_exit = 1;
            pthread_mutex_unlock(&exit_mutex);
            break;
        }
        pthread_mutex_unlock(&exit_mutex);
    }

    free(recv_buffer);
    return NULL;
}


int main( int argc, char** argv ){

    /*
    TODO:
    -->dinamically allocate memory in order to diminish program footprint on nvm
    -->multiple threaded approach when possible (one to recv and other to send)
    -->separate into other files src/ 
    --> add flags to verbose output 
    */ 

    if( 3 > argc ){
        fprintf(stderr, RED"%s\n"RESET, "USAGE:client <host> <port>");
        return 1;
    }

    #ifndef VERBOSE
    printf( "%s", YELLOW"[!] Getting address information...\n"RESET);
    #endif
    struct addrinfo hints, *server_addr;
    memset(&hints, 0, sizeof(hints)); //set all the addr with zero
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(argv[1], argv[2], &hints, &server_addr);
    if (status != 0) {
        fprintf(stderr, RED"[-] getaddrinfo error: %s\n"RESET, gai_strerror(status));
        freeaddrinfo(server_addr);
        return 1;
    }

    #ifndef VERBOSE
    printf( "%s", GREEN"[+] Address Information:\n");
    printf( "%s", "====================\n"); 
    print_addrinfo(server_addr);
    #endif


    #ifndef VERBOSE
    printf( "%s", YELLOW"[!] Creating socket... \n"RESET);
    #endif
    int sock_server = socket(server_addr->ai_family,
                                server_addr->ai_socktype, server_addr->ai_protocol);
    if ( sock_server < 0 ) {
        fprintf(stderr, RED"[-] socket error: %d\n"RESET, errno);
        freeaddrinfo(server_addr);
        return 1;
    }
    #ifndef VERBOSE
    printf( "%s", GREEN"[+] Socket created. \n"RESET);
    #endif
    


    printf( "%s", YELLOW"[!] Connecting to server... \n"RESET);
    status = connect(sock_server, server_addr->ai_addr, server_addr->ai_addrlen);
    if (status != 0) {
        fprintf(stderr, RED"[-] connect error: %d\n"RESET, errno);
        close(sock_server);
        freeaddrinfo(server_addr);
        return 1;
    }
    printf( "%s", GREEN"[+] Connected. \n"RESET);



    freeaddrinfo(server_addr);


    pthread_t send_tid, recv_tid;
    printf( "%s", YELLOW"[!] Creating sending thread... \n"RESET);
    status = pthread_create(&send_tid, NULL, send_thread, &sock_server);

    if ( status != 0 ){
        fprintf(stderr, RED"[-] pthread_create error: %d\n"RESET, status);
        close(sock_server);
        return 1;
    }
    printf( "%s", GREEN"[+] Sender thread created. \n"RESET);




    printf( "%s", YELLOW"[!] Creating receiving thread... \n"RESET);
    status = pthread_create(&recv_tid, NULL, recv_thread, &sock_server);

    if ( status != 0 ){
        fprintf(stderr, RED"[-] pthread_create error: %d\n"RESET, status);
        close(sock_server);
        return 1;
    }
    printf( "%s", GREEN"[+] Receiver thread created. \n"RESET);

    printf( "%s", YELLOW"[!] Waiting for threads to finish... \n"RESET);

    status = pthread_join(send_tid, NULL);
    if ( status != 0 ){
        fprintf(stderr, RED"[-] pthread_join error: %d\n"RESET, status);
        close(sock_server);
        return 1;
    }
    printf( "%s", GREEN"[+] Sender thread finished. \n"RESET);


    status = pthread_join(recv_tid, NULL);
    if ( status != 0 ){
        fprintf(stderr, RED"[-] pthread_join error: %d\n"RESET, status);
        close(sock_server);
        return 1;
    }
    printf( "%s", GREEN"[+] Receiver thread finished. \n"RESET);

    printf( "%s", GREEN"[+] All threads finished. \n"RESET);


    // Close the socket
    close(sock_server);


    return 0;
}

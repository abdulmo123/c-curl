#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define HEADER_SIZE 1024
#define BUFFER_SIZE 4096

char *to_upper(const char *string) {
    size_t len = strlen(string);
    char *s = malloc(len + 1);
    if (!s) return NULL;

    for (size_t i = 0; i < len; i++) {
        s[i] = toupper((unsigned char) string[i]);
    }

    s[len] = '\0';
    return s;
}

int is_digit(const char *string) {
    size_t len = strlen(string);

    if (!string) return 0;

    for (size_t i = 0; i < len; i++) {
        if (!isdigit(string[i])) {
            return 0;
        }
    }

    return 1;
}

void parse_args(int argc, char *argv[], char **protocol, char **host, char **port, char **path) {
    *protocol = malloc(20 * sizeof(char));
    *host = malloc(512 * sizeof(char));
    *port = malloc(8 * sizeof(char));
    *path = malloc(10 * sizeof(char));
    char *token;

    int i;

    for (i = 0; i < argc; i++) {
        if (strstr(argv[i], "http") != NULL) {
            break;
        }
    }

    int idx = 0;
    const char delimiters[] = "///:";
    char *s = strdup(argv[i]);
    token = strtok(s, delimiters);
    while (token != NULL) {
        if (idx == 0) {
            strcpy(*protocol, token);
        } else if (idx == 1) {
            strcpy(*host, token);
        } else if (is_digit(token)) {
            strcpy(*port, token);
        } else if ( (strcmp(token, "get") == 0) || 
        (strcmp(token, "put") == 0) || 
        (strcmp(token, "post") == 0) || 
        (strcmp(token, "delete") == 0) ) {
            strcpy(*path, token);
        }

        token = strtok(NULL, delimiters);
        idx++;
    }

    free(s);
    free(token);
}

void send_request(int argc, char *argv[], int client_fd, char *protocol, char *host, char *port, char *path, char *upper_path, char *upper_protocol) {
    char header[HEADER_SIZE];
    memset(header, 0, sizeof(header));
    int dash_v = 0;
    int dash_x = 0;
    int dash_d = 0;
    char *no_method = "";
    char *http_method = "";
    char *body = "";


    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            dash_v = 1;
        }
        if (strcmp(argv[i], "-X") == 0) {
            dash_x = 1;
            if ((strcmp(argv[i+1], "GET") == 0) || 
                (strcmp(argv[i+1], "PUT") == 0) || 
                (strcmp(argv[i+1], "POST") == 0) || 
                (strcmp(argv[i+1], "DELETE") == 0)) {
                    http_method = argv[i+1];
                    continue;
                }
            else {
                no_method = "Please specify the HTTP method!";
            }
        }

        if (strcmp(argv[i], "-d") == 0) {
            body = argv[i+1];
            printf("body = %s\n", body);
        }
    }

    if (dash_x) {
        if (strlen(no_method) != 0) {
            printf("%s\n", no_method);
            return;
        }
    }

    if (strcmp(http_method, "POST") == 0 || strcmp(http_method, "PUT") == 0) {
        snprintf(header, sizeof(header),
            "%s /%s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            upper_path, path, host, strlen(body), body
        );
    } else {
        snprintf(header, sizeof(header),
            "%s /%s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Accept: */*\r\n"
            "Connection: close\r\n"
            "\r\n", 
            upper_path, path, host
        );
    }
    // if (dash_v) {
    

    if (dash_v) {
        int i = 0;
        printf("> ");
        while (header[i] != '\0') {
            printf("%c", header[i]);

            if (header[i] == '\n') {
                printf("> ");
            }

            i++;
        }
        printf("\n");
    }

    else {
        printf("%s\n", header);
    }

    send(client_fd, header, strlen(header), 0);

    char buffer[BUFFER_SIZE];

    int n;
    if (dash_v) {
        int start_of_line = 1;
        while ((n = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
            // n = the size of the response
            buffer[n] = '\0'; // null-terminate at end of char[] / string
            for (int i = 0; i < n; i++) { // loop thru n 
                if (start_of_line) {
                    printf("< "); // if we are at start of a newline, print the char
                    start_of_line = 0; // set new line flag to 0 (false)
                }

                putchar(buffer[i]); // print character in buff array

                if (buffer[i] == '\n') {
                    start_of_line = 1; // if we are at a newline char, set flag to 1 (true)
                }
            }
        }
    } else {
        while ((n = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[n] = '\0';
            printf("%s", buffer);
        }
    }
}

void create_client_socket(int argc, char *argv[], char *protocol, char *host, char *port, char *path, char *upper_path, char *upper_protocol) {
    if (strlen(port) == 0) {
        port = "80";
    }

    struct addrinfo hints, *addrs;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int result = getaddrinfo(host, "80", &hints, &addrs);

    int client_fd;
    if ((client_fd = socket(addrs->ai_family, addrs->ai_socktype, 0)) < 0) {
        printf("Could not create socket\n");
        exit(EXIT_FAILURE);
    }

    if (connect(client_fd, addrs->ai_addr, addrs->ai_addrlen) < 0) {
        printf("Could not connect to socket\n");
        exit(EXIT_FAILURE);
    } 

    send_request(argc, argv, client_fd, protocol, host, port, path, upper_path, upper_protocol);

    close(client_fd);
}

int main (int argc, char *argv[]) {

    if (argc == 1) {
        return 0;
    }

    char *protocol, *host, *port, *path;
    parse_args(argc, argv, &protocol, &host, &port, &path);

    // printf("protocol = %s\n", &protocol);

    char *temp_path = path;
    char *temp_protocol = protocol;

    char *upper_path = to_upper(temp_path);
    char *upper_protocol = to_upper(temp_protocol);

    create_client_socket(argc, argv, protocol, host, port, path, upper_path, upper_protocol);

    free(protocol);
    free(host);
    free(port);
    free(path);
    free(upper_path);
    free(upper_protocol);

    protocol = NULL;
    host = NULL;
    port = NULL;
    path = NULL;
    upper_path = NULL;
    upper_protocol = NULL;
}
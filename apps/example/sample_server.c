// sample_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <pthread.h>

#define SERVER_PORT 8080
#define BACKLOG 128
#define BUF_SIZE 8192

static int running = 1;

void handle_sigint(int sig) {
    running = 0;
}

// Thread to handle each client
void *client_thread(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char buf[BUF_SIZE];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        close(client_fd);
        return NULL;
    }
    buf[n] = '\0';
    printf("=== Request ===\n%s\n", buf);

    // Extract file name from "GET /file"
    char filename[256] = {0};
    sscanf(buf, "GET /%255s", filename);
    if (strlen(filename) == 0) {
        strcpy(filename, "index.html");
    }

    // Try to open requested file
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        const char *resp404 =
            "HTTP/1.0 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "Connection: Close\r\n\r\n";
        write(client_fd, resp404, strlen(resp404));
        close(client_fd);
        return NULL;
    }

    struct stat st;
    fstat(fd, &st);

    // Send HTTP response header
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.0 200 OK\r\n"
        "Content-Length: %ld\r\n"
        "Connection: Close\r\n\r\n", st.st_size);
    write(client_fd, header, header_len);

    // Send file contents
    ssize_t r;
    while ((r = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t sent = 0;
        while (sent < r) {
            ssize_t w = write(client_fd, buf + sent, r - sent);
            if (w < 0) {
                perror("write");
                break;
            }
            sent += w;
        }
    }
    close(fd);
    close(client_fd);
    printf("Finished serving %s\n", filename);
    return NULL;
}

int main() {
    signal(SIGINT, handle_sigint);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr("10.2.0.1");

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Linux TCP server running on port %d\n", SERVER_PORT);

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
        printf("Accepted connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));   
        if (*client_fd < 0) {
            if (errno == EINTR) break; // interrupted by SIGINT
            perror("accept");
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, client_fd);
        pthread_detach(tid);
    }

    close(server_fd);
    printf("Server shutting down\n");
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP   "10.200.1.1"  // server IP
#define SERVER_PORT 8080
#define BUF_SIZE    8192

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUF_SIZE];
    int n;

    // create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    // server address
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    // connect
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        exit(1);
    }

    // send HTTP GET
    char *req = "GET /sample_100MB.txt HTTP/1.0\r\nHost: 10.200.1.1\r\n\r\n";
    if (write(sock, req, strlen(req)) < 0) {
        perror("write");
        close(sock);
        exit(1);
    }

    // open output file
    FILE *fp = fopen("downloaded.txt", "wb");
    if (!fp) {
        perror("fopen");
        close(sock);
        exit(1);
    }

    // receive and save
    while ((n = read(sock, buffer, BUF_SIZE)) > 0) {
        fwrite(buffer, 1, n, fp);
        printf("received: %d\n",n);
    }

    fclose(fp);
    close(sock);

    printf("Download complete: saved to downloaded_100MB.txt\n");
    return 0;
}

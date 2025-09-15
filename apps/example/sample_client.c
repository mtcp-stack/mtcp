#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "10.200.1.1" // server IP
#define SERVER_PORT 8080
#define BUF_SIZE (64 * 1024)

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUF_SIZE];
    int n;
    char *file_name = "random_100MB"; // default file name
    // create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }

    // server address
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    int size = 1 << 20; // 1 MB
    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) < 0)
    {
        perror("setsockopt SO_RCVBUF");
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) < 0)
    {
        perror("setsockopt SO_SNDBUF");
    }

    // connect
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("connect");
        exit(1);
    }

    // send HTTP GET
    // char *req = "GET /random_10MB.txt HTTP/1.0\r\nHost: 10.200.1.1\r\n\r\n";
    // char *req = "GET /random_100MB.txt HTTP/1.0\r\nHost: 10.200.1.1\r\n\r\n";
    char req[1024];
    snprintf(req, sizeof(req), "GET /%s_%s.txt HTTP/1.0\r\nHost: %s\r\n\r\n", file_name, argv[1], SERVER_IP);
    printf("Request:\n%s", req);
    if (write(sock, req, strlen(req)) < 0)
    {
        perror("write");
        close(sock);
        exit(1);
    }

    // open output file
    char down[1024];
    char *downloading_file_name = "downloaded_100MB";
    snprintf(down, sizeof(down), "%s_%s.txt", downloading_file_name, argv[1]);
    fprintf(stderr,"Saving to file: %s\n", down);
    FILE *fp = fopen(down, "wb");
    if (!fp)
    {
        perror("fopen");
        close(sock);
        exit(1);
    }

    // receive and save
    while ((n = read(sock, buffer, BUF_SIZE)) > 0)
    {
        fwrite(buffer, 1, n, fp);
        printf("received: %d\n", n);
        fflush(fp);
    }

    fclose(fp);
    close(sock);

    printf("Download complete: saved to downloaded_100MB.txt\n");
    return 0;
}

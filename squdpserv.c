#include <errno.h>
#include <err.h>
#include <sysexits.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libgen.h>

#include "sqserv.h"

// Define FD_COPY if it's not already defined
#ifndef FD_COPY
#define FD_COPY(orig, dest) memcpy((dest), (orig), sizeof(*(dest)))
#endif

char *prog_name;

void usage()
{
    printf("usage: %s [-p <port>]\n", prog_name);
    printf("\t-p <port> specify alternate port\n");
}

int square(int number)
{
    return number * number;
}

int main(int argc, char *argv[])
{
    long port = DEFAULT_PORT;
    struct sockaddr_in sin, cliaddr;
    int passive_sock, new_sock, nb_set, max_fd;
    fd_set rd_mask, wr_mask, ex_mask, rd_sel, wr_sel, ex_sel;
    socklen_t clilen;
    size_t data_len;
    ssize_t len;
    char *data;
    long ch;
    int queueLength = 5;
    int i, client_sockets[FD_SETSIZE];

    /* get options and arguments */
    prog_name = strdup(basename(argv[0]));
    while ((ch = getopt(argc, argv, "?hp:")) != -1)
    {
        switch (ch)
        {
        case 'p':
            port = strtol(optarg, (char **)NULL, 10);
            break;
        case 'h':
        case '?':
        default:
            usage();
            return 0;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 0)
    {
        usage();
        return EX_USAGE;
    }

    printf("listening on port %d\n", (int)port);

    /* get room for data */
    data_len = BUFFER_SIZE;
    if ((data = (char *)malloc(data_len)) < 0)
    {
        err(EX_SOFTWARE, "in malloc");
    }

    /* create and bind a socket */
    passive_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (passive_sock < 0)
    {
        free(data);
        err(EX_SOFTWARE, "in socket");
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY; // rather memcpy to sin.sin_addr
    sin.sin_port = htons(port);

    if (bind(passive_sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        free(data);
        close(passive_sock);
        err(EX_SOFTWARE, "in bind");
    }
    // tell OS to receive and queue SYN packet
    listen(passive_sock, queueLength);
    clilen = sizeof(cliaddr);

    // initialize client socket array
    for (i = 0; i < FD_SETSIZE; i++)
    {
        client_sockets[i] = -1;
    }

    // main loop
    max_fd = passive_sock;
    FD_ZERO(&rd_mask);
    FD_SET(passive_sock, &rd_mask);
    FD_ZERO(&wr_mask);
    FD_ZERO(&ex_mask);
    FD_SET(passive_sock, &ex_mask);

    while (1)
    {
        FD_COPY(&rd_mask, &rd_sel);
        FD_COPY(&wr_mask, &wr_sel);
        FD_COPY(&ex_mask, &ex_sel);

        nb_set = select(max_fd + 1, &rd_sel, &wr_sel, &ex_sel, NULL);

        if (nb_set < 0)
        {
            perror("select");
            // probably return or exit
        }

        if (nb_set > 0)
        {
            if (FD_ISSET(passive_sock, &rd_sel))
            {
                new_sock = accept(passive_sock, (struct sockaddr *)&cliaddr, &clilen);

                if (new_sock < 0)
                {
                    perror("ERROR on accept");
                }
                else
                {
                    for (i = 0; i < FD_SETSIZE; i++)
                    {
                        if (client_sockets[i] < 0)
                        {
                            client_sockets[i] = new_sock;
                            if (new_sock > max_fd)
                            {
                                max_fd = new_sock;
                            }
                            FD_SET(new_sock, &rd_mask);
                            FD_SET(new_sock, &wr_mask);
                            FD_SET(new_sock, &ex_mask);
                            break;
                        }
                    }
                    if (i == FD_SETSIZE)
                    {
                        fprintf(stderr, "Too many clients\n");
                        close(new_sock);
                    }
                }
            }
            else
            {
                for (i = 0; i < FD_SETSIZE; i++)
                {
                    if (client_sockets[i] >= 0 && FD_ISSET(client_sockets[i], &rd_sel))
                    {
                        // Handle data from client
                        memset(data, 0, data_len);
                        len = recv(client_sockets[i], data, data_len, 0);
                        if (len < 0)
                        {
                            perror("ERROR in recvfrom");
                        }
                        else if (len == 0)
                        {
                            // Connection closed by client
                            close(client_sockets[i]);
                            FD_CLR(client_sockets[i], &rd_mask);
                            FD_CLR(client_sockets[i], &wr_mask);
                            FD_CLR(client_sockets[i], &ex_mask);
                            client_sockets[i] = -1;
                        }
                        else
                        {
                            // Process data and send response
                            printf("Received data from client %d: %s\n", i, data);

                            // Convert the received string to a long integer
                            char *end;
                            long received_number = strtol(data, &end, 10);

                            // Square the received number
                            int result = square(received_number);

                            // Convert the result back to a string and send it back to the client
                            memset(data, 0, data_len);
                            snprintf(data, data_len, "%d", result);
                            len = send(client_sockets[i], data, strlen(data), 0);
                            if (len < 0)
                            {
                                perror("ERROR in sending response");
                            }
                        }
                    }
                }
            }
        }
    }
    free(data);
    close(passive_sock);
    printf("Socket closed\n");
    return EX_OK;
}
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

#undef LEGACY

char *prog_name;

void usage()
{
    printf("usage: %s [-p <port>] <hostname>\n", prog_name);
    printf("\tremote host name as argument\n");
    printf("\t-p <port> specify alternate port\n");
}

int main(int argc, char *argv[])
{

    long port = DEFAULT_PORT;
    struct sockaddr_in sin;
#ifdef LEGACY
    struct hostent *hp;
#else
    struct addrinfo hints, *ai;
    char service[15];
    char ipstr[INET6_ADDRSTRLEN];
    int error;
#endif
    int fd;
    size_t data_len;
    ssize_t len;
    char *data;
    char *host;
    long ch;

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

    if (argc != 1)
    {
        usage();
        return EX_USAGE;
    }

    host = argv[0];

    printf("sending to %s:%d\n", host, (int)port);

    data_len = BUFFER_SIZE;
    if ((data = (char *)malloc(data_len)) < 0)
    {
        err(EX_SOFTWARE, "in malloc");
    }
    /* create a socket, no bind needed, let the system choose */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        free(data);
        err(EX_SOFTWARE, "in socket");
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY; // rather memcpy to sin.sin_addr
    sin.sin_port = htons(port);
    /* gethostbyname for information, getaddrinfo is prefered */
#ifdef LEGACY
    if ((hp = gethostbyname(host)) == NULL)
    {
        errx(EX_NOHOST, "(gethostbyname) cannot resolve %s: %s", host, hstrerror(h_errno));
    }
    printf("gethostbyname: resolved name '%s' to IP address %s\n", hp->h_name, inet_ntoa(*(struct in_addr *)hp->h_addr));
#else
    /* getaddrinfo is the prefered method today */
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_ADDRCONFIG | AI_CANONNAME | AI_NUMERICSERV; // AI_PASSIVE
    hints.ai_family = AF_UNSPEC;                                    // AF_INET AF_INET6
    hints.ai_socktype = SOCK_STREAM;
    snprintf(service, sizeof(service), "%ld", port);
    if ((error = getaddrinfo(host, service, &hints, &ai)) != 0)
    {
        errx(EX_NOHOST, "(getaddrinfo) cannot resolve %s: %s", host, gai_strerror(error));
    }
    for (; ai != NULL; ai = ai->ai_next)
    {
        switch (ai->ai_family)
        {
        case AF_INET:
            inet_ntop(ai->ai_family, &(((struct sockaddr_in *)ai->ai_addr)->sin_addr), ipstr, sizeof(ipstr));
            break;
        case AF_INET6:
            inet_ntop(ai->ai_family, &(((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr), ipstr, sizeof(ipstr));
            break;
        default:
            errx(EX_NOHOST, "(getaddrinfo) unknown address family: %d", ai->ai_family);
        }
        printf("getaddrinfo:   resolved name '%s' to IP address %s\n", ai->ai_canonname, ipstr);
    }
#endif
    /*establish connection to server*/
    if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("ERROR connecting");
        close(fd);
        err(EX_SOFTWARE, "ERROR connecting");
    }
    /* send data */
    printf("Value to square: ");
    scanf("%ld", &ch);
    snprintf(data, data_len, "%ld", ch);
    send(fd, data, strlen(data), 0);
    /* receive data */
    len = recv(fd, data, data_len, 0);
    if (len < 0)
    {
        free(data);
        close(fd);
        err(EX_SOFTWARE, "in recv");
    }
    else
    {
        printf("integer value: %s\n", data);
    }
    free(data);
    close(fd);
    printf("Socked closed\n");
    return EX_OK;
}

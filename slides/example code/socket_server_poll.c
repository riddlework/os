#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <poll.h>



int recv_one_message(int sock);



int main()
{
    int server = socket(AF_INET, SOCK_STREAM, 0);

    /* this tells Linux that we're OK with rapidly re-using old sockets.  If we
     * don't do this, then it is disallowed for a while (maybe 1 minute?)
     *    https://stackoverflow.com/questions/24194961/how-do-i-use-setsockoptso-reuseaddr
     */
    const int enable = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    struct sockaddr_in listen_addr;
    listen_addr.sin_family      = AF_INET;
    listen_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    listen_addr.sin_port        = htons(CHOOSE_A_PORT);

    int rc = bind(server, (struct sockaddr*)&listen_addr, sizeof(listen_addr));
    assert(rc == 0);

    rc = listen(server, 5);
    assert(rc == 0);

    struct pollfd fds[1024];
    memset(fds, 0, sizeof(fds));
    int num_fds = 0;

    /* the first socket to add to the poll() listen is the server itself. */
    fds[0].fd     = server;
    fds[0].events = POLLIN;
    num_fds++;

    while (1)
    {
        printf("Calling poll().  The process will block until something has happened.\n");
        int poll_rc = poll(fds, num_fds, -1);
        assert(poll_rc > 0);

        /* check the server socket first.  This tells us that there is an incoming
         * connection to accept.
         */
        if (fds[0].revents != 0)
        {
            /* we're not expecting any events except for POLLIN.  In error
             * scenarios, can we get other types?  TODO.
             */
            assert(fds[0].revents == POLLIN);

            int connected_sock = accept(server, NULL,NULL);
            printf("A new client has connected!  Adding the socket to the fds[] array...\n");

            assert(num_fds < 1024);   // TODO: implement re-use of slots
            fds[num_fds].fd      = connected_sock;
            fds[num_fds].events  = POLLIN;
            fds[num_fds].revents = 0;      // so we won't get spurious events immediately
            num_fds++;
        }

        for (int i=1; i<num_fds; i++)
        {
            if (fds[i].revents != 0)
            {
                printf("i=%d fd=%d revents=%d\n", i, fds[i].fd, fds[i].revents);
                assert(fds[i].revents == POLLIN);

                int rc = recv_one_message(fds[i].fd);
                if (rc == 0)
                {
                    printf("i=%d fd=%d - returned 0.  This means that the other end of the socket has closed the socket.  We will clean up the socket to match.\n", i, fds[i].fd);
                    close(fds[i].fd);
                    fds[i].fd = -1;
                }
            }
        }
    }

    return -1;  // we never get here
}



int recv_one_message(int sock)
{
    printf("worker(sock=%d): Receiving a message from the client.\n", sock);

    char buf[256];
    int rc = recv(sock, buf,sizeof(buf)-1, 0);
    printf("worker(sock=%d): %d bytes received.\n", sock, rc);
    if (rc == 0)
    {
        printf("worker(sock=%d): rc == 0; this indicates that the socket has closed.\n", sock);
        return 0;
    }

    buf[rc] = '\0';
    printf("worker(sock=%d): Data received -- '%s'\n", sock, buf);

    sprintf(buf, "I just received %d bytes from you.", (int)strlen(buf));
    printf("worker(sock=%d): Sending the following buffer -- '%s'\n", sock, buf);

    rc = send(sock, buf,strlen(buf), 0);
    printf("worker(sock=%d): %d bytes sent, of %d bytes attempted\n", sock, rc, (int)strlen(buf));

    return rc;
}


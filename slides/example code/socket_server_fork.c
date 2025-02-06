#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>



void worker(int);



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

    /* this tells Linux that we don't care about child status, so our children
     * get cleaned up automatically; they don't become zombies.
     */
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags   = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sa, NULL);

    while (1)
    {
        printf("Waiting for another connection from a client...\n");
        int connected_sock = accept(server, NULL,NULL);

        printf("A client was connected.  fork()ing of a child process to handle it.\n");
        fflush(NULL);
        int fork_rc = fork();

        if (fork_rc == 0)
        {
            printf("In the child process, pid %d.  Closing the server socket.\n", getpid());
            close(server);

            printf("In the child process.  Calling the worker function to handle the connection...\n");
            worker(connected_sock);

            return 0;
        }
        else
        {
            printf("In the parent.  Created a child, pid %d.  Closing the connected socket.\n", fork_rc);
            close(connected_sock);
        }
    }

    return -1;  // we never get here
}



void worker(int sock)
{
    printf("worker(sock=%d): Receiving a message from the client.\n", sock);

    char buf[256];
    int rc = recv(sock, buf,sizeof(buf)-1, 0);
    printf("worker(sock=%d): %d bytes received.\n", sock, rc);
    assert(rc > 0);

    buf[rc] = '\0';
    printf("worker(sock=%d): Data received -- '%s'\n", sock, buf);

    sprintf(buf, "This is a message that I'll be sending to the client.");
    printf("worker(sock=%d): Sending the following buffer -- '%s'\n", sock, buf);

    rc = send(sock, buf,strlen(buf), 0);
    printf("worker(sock=%d): %d bytes sent, of %d bytes attempted\n", sock, rc, (int)strlen(buf));

    printf("worker(sock=%d): Worker is done with its work; closing the socket.\n", sock);
    close(sock);

    printf("\n");   // a blank line for visual spacing of the output
    return;
}


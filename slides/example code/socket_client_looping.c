#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>



int main()
{
    int client = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in conn_addr;
    conn_addr.sin_family      = AF_INET;
    conn_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    conn_addr.sin_port        = htons(CHOOSE_A_PORT);

    int rc = connect(client, (struct sockaddr*)&conn_addr, sizeof(conn_addr));
    assert(rc == 0);

    /* see worker() in the server to understand the rest of this function */

    while (1)
    {
        char buf[256];

        printf("Reading a line of input from stdin...\n");
        char *fgets_rc = fgets(buf, sizeof(buf)-1, stdin);
        if (fgets_rc == NULL)
        {
            printf("EOF or error hit on stdin.\n");
            break;
        }
        printf("%d characters read.\n", (int)strlen(buf));

        printf("Sending the buffer to the server.\n");
        rc = send(client, buf,strlen(buf), 0);
        printf("%d bytes sent.\n", rc);

        printf("Receiving the reply buffer from the server.\n");
        rc = recv(client, buf,sizeof(buf)-1, 0);
        printf("%d bytes received.\n", rc);

        printf("Data received -- '%s'\n", buf);
    }

    return 0;
}


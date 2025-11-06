#include "server/server.h"

#include <csignal>

#define DEFAULT_SERVER_PORT 8000

RSHookServer g_server;

void sigint_handler(int signo)
{
    g_server.shutdown();

    printf("shutdown\n");
    exit(0);
}

bool setup_listening_socket(int port, int& sock)
{
    struct sockaddr_in srv_addr;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        return false;
    }

    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        return false;
    }

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (const struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0) {
        false;
    }
    
    if (listen(sock, 128) < 0) {
        return false;
    }

    return true;
}

int main(int argc, char **argv)
{
    int server_socket_fd;
    setup_listening_socket(DEFAULT_SERVER_PORT, server_socket_fd);
    g_server.startup(DEFAULT_SERVER_PORT, server_socket_fd);

    //setup signal handler for graceful shutdown
    signal(SIGINT, sigint_handler);

    g_server.runloop();
    return 0;
}

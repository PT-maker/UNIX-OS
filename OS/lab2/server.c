#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_PORT 5000
#define BACKLOG 16

static volatile sig_atomic_t was_sighup = 0;
static volatile sig_atomic_t was_stop = 0;

static void sighup_handler(int signal_number)
{
    (void)signal_number;
    was_sighup = 1;
}

static void stop_handler(int signal_number)
{
    (void)signal_number;
    was_stop = 1;
}

static int set_handler(int signal_number, void (*handler)(int))
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    return sigaction(signal_number, &action, NULL);
}

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;
    if (argc > 1)
        port = atoi(argv[1]);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int yes = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt");
        close(server_fd);
        return EXIT_FAILURE;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (set_handler(SIGHUP, sighup_handler) == -1 ||
        set_handler(SIGINT, stop_handler) == -1 ||
        set_handler(SIGTERM, stop_handler) == -1) {
        perror("sigaction");
        close(server_fd);
        return EXIT_FAILURE;
    }

    sigset_t blocked_mask, original_mask;
    sigemptyset(&blocked_mask);
    sigaddset(&blocked_mask, SIGHUP);
    sigaddset(&blocked_mask, SIGINT);
    sigaddset(&blocked_mask, SIGTERM);

    if (sigprocmask(SIG_BLOCK, &blocked_mask, &original_mask) == -1) {
        perror("sigprocmask");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("Сервер слушает порт %d (PID %d)\n", port, (int)getpid());
    fflush(stdout);

    int client_fd = -1;

    while (!was_stop) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;
        if (client_fd != -1) {
            FD_SET(client_fd, &read_fds);
            if (client_fd > max_fd)
                max_fd = client_fd;
        }

        int ready = pselect(max_fd + 1, &read_fds, NULL, NULL, NULL, &original_mask);

        if (was_sighup) {
            was_sighup = 0;
            printf("Получен сигнал SIGHUP\n");
            fflush(stdout);
        }

        if (ready == -1) {
            if (errno == EINTR)
                continue;
            perror("pselect");
            break;
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in peer;
            socklen_t peer_len = sizeof(peer);
            int new_fd = accept(server_fd, (struct sockaddr *)&peer, &peer_len);
            if (new_fd == -1) {
                if (errno != EINTR && errno != ECONNABORTED && errno != EAGAIN)
                    perror("accept");
            } else {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
                printf("Новое подключение от %s:%d\n", ip, ntohs(peer.sin_port));
                fflush(stdout);

                if (client_fd == -1) {
                    client_fd = new_fd;
                } else {
                    close(new_fd);
                    printf("Соединение закрыто: уже есть активный клиент\n");
                    fflush(stdout);
                }
            }
        }

        if (client_fd != -1 && FD_ISSET(client_fd, &read_fds)) {
            char buffer[4096];
            ssize_t received = recv(client_fd, buffer, sizeof(buffer), 0);
            if (received > 0) {
                printf("Получено байтов: %zd\n", received);
                fflush(stdout);
            } else if (received == 0 || (errno != EINTR && errno != EAGAIN)) {
                if (received == -1)
                    perror("recv");
                printf("Клиент отключился\n");
                fflush(stdout);
                close(client_fd);
                client_fd = -1;
            }
        }
    }

    printf("Завершение работы\n");
    if (client_fd != -1)
        close(client_fd);
    close(server_fd);
    sigprocmask(SIG_SETMASK, &original_mask, NULL);
    return EXIT_SUCCESS;
}
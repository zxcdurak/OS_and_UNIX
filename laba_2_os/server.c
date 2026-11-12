#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT    5000
#define BACKLOG 16

volatile sig_atomic_t was_sighup = 0;
volatile sig_atomic_t was_sigterm = 0;

void sighup_handler(int signo) {
    (void)signo;
    was_sighup = 1;
}

void sigterm_handler(int signo) {
    (void)signo;
    was_sigterm = 1;
}

int create_server_socket(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }

    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, BACKLOG) == -1) {
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

int setup_signals(sigset_t *orig_mask) {
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGHUP);
    sigaddset(&blocked, SIGTERM);
    sigaddset(&blocked, SIGINT);
    if (sigprocmask(SIG_BLOCK, &blocked, orig_mask) == -1) {
        perror("sigprocmask");
        return -1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sa.sa_handler = sighup_handler;
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction SIGHUP");
        return -1;
    }
    sa.sa_handler = sigterm_handler;
    if (sigaction(SIGTERM, &sa, NULL) == -1 || sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGTERM/SIGINT");
        return -1;
    }
    return 0;
}

void handle_new_connection(int server_fd, int *client_fd) {
    int fd = accept(server_fd, NULL, NULL);
    if (fd == -1) {
        perror("accept");
        return;
    }
    if (*client_fd == -1) {
        *client_fd = fd;
        printf("Новое соединение (fd %d): принято\n", fd);
    } else {
        printf("Новое соединение (fd %d): закрыто, клиент уже подключён\n", fd);
        close(fd);
    }
    fflush(stdout);
}

void handle_client_data(int *client_fd) {
    char buf[4096];
    ssize_t n = recv(*client_fd, buf, sizeof(buf), 0);
    if (n > 0) {
        printf("Получено байт: %zd\n", n);
    } else {
        if (n == 0)
            printf("Клиент отключился\n");
        else
            perror("recv");
        close(*client_fd);
        *client_fd = -1;
    }
    fflush(stdout);
}

int main(void) {
    int server_fd = create_server_socket(PORT);
    if (server_fd == -1)
        return EXIT_FAILURE;

    sigset_t orig_mask;
    if (setup_signals(&orig_mask) == -1) {
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("Сервер запущен: PID %ld, порт %d\n", (long)getpid(), PORT);
    fflush(stdout);

    int client_fd = -1;
    while (!was_sigterm) {
        fd_set rfds; 
        FD_ZERO(&rfds);
        FD_SET(server_fd, &rfds);
        int max_fd = server_fd;
        if (client_fd != -1) {
            FD_SET(client_fd, &rfds);
            if (client_fd > max_fd)
                max_fd = client_fd;
        }

        int ready = pselect(max_fd + 1, &rfds, NULL, NULL, NULL, &orig_mask);
        if (ready == -1) {
            if (errno != EINTR) {
                perror("pselect");
                break;
            }
            if (was_sighup) {             
                was_sighup = 0;
                printf("Получен сигнал SIGHUP\n");
                fflush(stdout);
            }
            continue;                  
        }

        if (FD_ISSET(server_fd, &rfds))
            handle_new_connection(server_fd, &client_fd);
        if (client_fd != -1 && FD_ISSET(client_fd, &rfds))
            handle_client_data(&client_fd);
    }

    printf("Завершение работы\n");
    if (client_fd != -1) close(client_fd);
    close(server_fd);
    sigprocmask(SIG_SETMASK, &orig_mask, NULL);
    return EXIT_SUCCESS;
}
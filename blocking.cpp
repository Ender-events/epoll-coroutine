#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>

int new_listen(struct addrinfo* addr_res) {
    struct addrinfo hints;

    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // fill in my IP for me

    getaddrinfo(nullptr, "3490", &hints, &addr_res);
    auto fd = socket(addr_res->ai_family, addr_res->ai_socktype, addr_res->ai_protocol);
    int opt;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (bind(fd, addr_res->ai_addr, addr_res->ai_addrlen) == -1)
        throw std::runtime_error{"bind"};
    listen(fd, 8);
    return fd;
}
void echo_socket(int socket) {
    while (true) {
        char buffer[42] = {0};
        ssize_t nbRecv = recv(socket, buffer, sizeof(buffer), 0);
        ssize_t nbSend = 0;
        while (nbSend < nbRecv) {
            ssize_t res = send(socket, buffer, sizeof(buffer), 0);
            if (res <= 0)
                return;
            nbSend += res;
        }
        if (nbRecv <= 0)
            return;
    }
}
void accept(int listen) {
    try {
        while (true) {
            struct sockaddr_storage their_addr;
            socklen_t addr_size = sizeof their_addr;
            auto socket = accept(listen, (struct sockaddr*)&their_addr, &addr_size);
            echo_socket(socket);
            close(socket);
        }
    } catch (const std::exception& e) {
        std::cout << "exception (accept): " << e.what() << '\n';
    }
}
int main() {
    struct addrinfo* addr_res = nullptr;
    auto listen = new_listen(addr_res);
    accept(listen);
    freeaddrinfo(addr_res);
    close(listen);
}

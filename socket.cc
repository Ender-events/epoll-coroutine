#include "socket.hh"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

Socket::Socket(std::string_view port, IOContext& io_context)
    : io_context_{io_context}
{
    struct addrinfo hints, *res;

    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    getaddrinfo(NULL, port.data(), &hints, &res);
    fd_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    int opt;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (bind(fd_, res->ai_addr, res->ai_addrlen) == -1)
        throw std::runtime_error{"bind"};
    listen(fd_, 8);
    fcntl(fd_, F_SETFL, O_NONBLOCK);
    io_context_.attach(this);
    io_context_.watchRead(this);
}

Socket::Socket(Socket&& socket)
    : io_context_{socket.io_context_}
    , fd_{socket.fd_}
    , io_state_{socket.io_state_}
    , io_new_state_{socket.io_new_state_}
{
    socket.fd_ = -1;
}

Socket::~Socket()
{
    if (fd_ == -1)
        return;
    io_context_.detach(this);
    std::cout << "close(" << fd_ << ")\n";
    close(fd_);
}

std::task<std::shared_ptr<Socket>> Socket::accept()
{
    int fd = co_await SocketAcceptOperation{this};
    if (fd == -1)
        throw std::runtime_error{"accept"};
    co_return std::shared_ptr<Socket>(new Socket{fd, io_context_});
}

SocketRecvOperation Socket::recv(void* buffer, std::size_t len)
{
    return SocketRecvOperation{this, buffer, len};
}
SocketSendOperation Socket::send(void* buffer, std::size_t len)
{
    return SocketSendOperation{this, buffer, len};
}

Socket::Socket(int fd, IOContext& io_context)
    : io_context_{io_context}
    , fd_{fd}
{
    fcntl(fd_, F_SETFL, O_NONBLOCK);
    io_context_.attach(this);
}

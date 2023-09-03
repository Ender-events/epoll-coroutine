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
    struct addrinfo hints;

    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // fill in my IP for me

    getaddrinfo(nullptr, port.data(), &hints, &addr_res);
    fd_ = socket(addr_res->ai_family, addr_res->ai_socktype, addr_res->ai_protocol);
    int opt;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (bind(fd_, addr_res->ai_addr, addr_res->ai_addrlen) == -1)
        throw std::runtime_error{"bind"};
    listen(fd_, 8);
    fcntl(fd_, F_SETFL, O_NONBLOCK);
    io_context_.attach(this);
    io_context_.watchRead(this);
}

Socket::Socket(Socket&& socket) noexcept
    : io_context_{socket.io_context_}
    , fd_{socket.fd_}
    , io_state_{socket.io_state_}
    , io_new_state_{socket.io_new_state_}
{
    socket.fd_ = -1;
    std::swap(addr_res, socket.addr_res);
}

Socket::~Socket()
{
    if (fd_ == -1)
        return;
    io_context_.detach(this);
    freeaddrinfo(addr_res);
    std::cout << "close(" << fd_ << ")\n";
    close(fd_);
}

std::lazy<std::shared_ptr<Socket>> Socket::accept()
{
    std::cout << "co_await SocketAcceptOperation\n";
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

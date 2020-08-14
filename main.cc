#include "io_context.hh"
#include "socket.hh"

int main()
{
    IOContext io_context{};
    Socket listen{"3490", io_context};

    io_context.run();
}

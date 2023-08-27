CXX=g++-12
LINK.o = $(LINK.cc)
# CXXFLAGS=-Wall -Wextra -Werror -pedantic -std=c++20 -fcoroutines -g
CXXFLAGS=-Wall -Wextra -Werror -pedantic -std=c++20 -fcoroutines -g -fsanitize=address
LD=g++-10

OBJ = main.o io_context.o socket_accept_operation.o socket_recv_operation.o socket_send_operation.o socket.o

main: ${OBJ}

clean:
	${RM} ${OBJ}

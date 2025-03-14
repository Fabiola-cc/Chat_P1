#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>

using namespace boost::asio;
using namespace boost::beast;
using tcp = ip::tcp;

int main() {
    io_context ioc;
    tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 8080));

    std::cout << "Servidor esperando conexiones en el puerto 8080...\n";
    while (true) {
        tcp::socket socket(ioc);
        acceptor.accept(socket);
        std::cout << "Cliente conectado\n";
    }
}

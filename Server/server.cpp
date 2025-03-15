#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>

namespace beast = boost::beast;  
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using namespace std;

void do_session(tcp::socket& socket) {
    try {
        // Crear un WebSocket stream
        websocket::stream<tcp::socket> ws(std::move(socket));

        // Realizar el handshake
        ws.accept();

        std::cout << "Cliente conectado y sesión WebSocket iniciada...\n";

        // Bucle para mantener la conexión abierta y recibir mensajes
        while (true) {
            beast::multi_buffer buffer;
            
            // Recibir el mensaje del cliente
            ws.read(buffer);

            // Mostrar el mensaje recibido en la consola
            std::cout << "Mensaje recibido: " << beast::buffers_to_string(buffer.data()) << std::endl;

            // Enviar un mensaje de vuelta al cliente
            std::string message = "Mensaje recibido!";
            ws.write(net::buffer(message));
        }

    } catch (const std::exception& e) {
        std::cerr << "Error en la sesión: " << e.what() << std::endl;
    }
}

int main() {
    try {
        net::io_context ioc;

        // Aceptar conexiones en el puerto 8080
        tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 8080));
        std::cout << "Servidor esperando conexiones WebSocket en el puerto 8080...\n";

        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            std::cout << "Cliente conectado\n";
            
            // Manejar la sesión WebSocket en un hilo separado
            std::thread{std::bind(&do_session, std::move(socket))}.detach();
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}

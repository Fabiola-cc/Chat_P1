#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <string>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using namespace std;

// Diccionario global de usuarios (nombre -> estado)
unordered_map<string, int> users;
mutex users_mutex; // Mutex para proteger el acceso al diccionario

// Función para extraer el nombre del usuario de la URL WebSocket
string extract_username(const beast::http::request<beast::http::string_body>& req) {
    string target(req.target());
    size_t pos = target.find("?name=");

    if (pos != string::npos) {
        return target.substr(pos + 6); // Extraer el nombre de usuario después de "?name="
    }
    return "Desconocido"; // Si no se proporciona un nombre
}

// Función para imprimir la lista de usuarios conectados
void print_users() {
    lock_guard<mutex> lock(users_mutex);
    cout << "Usuarios disponibles [" << users.size() << "]: ";
    for (const auto& [user, status] : users) {
        cout << user << ", " << status << " |";
    }
    cout << endl;
}

void do_session(tcp::socket socket) {
    string username; // Declaramos username antes del try

    try {
        websocket::stream<tcp::socket> ws(move(socket));

        // Realizar el handshake manual para obtener la URL de conexión
        beast::flat_buffer buffer;
        beast::http::request<beast::http::string_body> req;
        beast::http::read(ws.next_layer(), buffer, req);

        username = extract_username(req); // Obtener el nombre del usuario

        {
            lock_guard<mutex> lock(users_mutex);

            if (users.find(username) == users.end()) {
                // Si el usuario no está en la lista, crear un nuevo registro con estado 1 (conectado)
                users[username] = 1;
                cout << "✅ Nuevo usuario conectado: " << username << endl;
                ws.accept(req); // Completar el handshake solo si el usuario es nuevo
            } else {
                // Si el usuario está en la lista, verificar el estado
                if (users[username] == 0) {
                    // Si el usuario está desconectado, cambiar el estado a 1 (conectado)
                    users[username] = 1;
                    cout << "✅ Usuario reconectado: " << username << endl;
                    ws.accept(req); // Completar el handshake solo si el usuario estaba desconectado
                } else {
                    // Si el usuario ya está conectado (estado 1), regresar un mensaje de error
                    cout << "⚠️ Intento de reconexión fallido, usuario ya esta regisrado y no esta desconectado: " << username << endl;
                    ws.close(websocket::close_code::normal);
                    return;  // Salir de la función, no procesamos más
                }
            }
        }
        
        // Completar el handshake solo si el usuario ha sido aceptado
        print_users();

        while (true) {
            beast::multi_buffer buffer;
            ws.read(buffer);
            string message = beast::buffers_to_string(buffer.data());

            cout << "💬 Mensaje de " << username << ": " << message << endl;

            // Enviar una respuesta de confirmación
            ws.write(net::buffer("Hola " + username + ", tu mensaje fue recibido."));
        }

    } catch (const std::exception& e) {
        cerr << "⚠️ Error en la sesión: " << e.what() << endl;
    }

    // Cuando la sesión termine, asegurarse de marcar al usuario como desconectado
    if (!username.empty()) { // Verificamos que no sea una cadena vacía
        {
            lock_guard<mutex> lock(users_mutex);
            users[username] = 0; // Estado 0 = desconectado
        }
        cout << "❌ Usuario desconectado: " << username << endl;
        print_users();
    }
}


int main() {
    try {
        net::io_context ioc;
        tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 8080));
        cout << "🌐 Servidor WebSocket en el puerto 8080...\n";

        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            cout << "🔗 Cliente conectado\n";

            // Manejar la sesión en un hilo separado
            thread{do_session, move(socket)}.detach();
        }

    } catch (const exception& e) {
        cerr << "❌ Error: " << e.what() << endl;
    }

    return 0;
}

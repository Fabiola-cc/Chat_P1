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

void send_users_list(const string& sender, websocket::stream<tcp::socket>& ws) {
    lock_guard<mutex> lock(users_mutex);

    vector<uint8_t> response;
    response.push_back(51);  // Código de mensaje (respuesta a listar usuarios)
    response.push_back(users.size());  // Número de usuarios

    for (const auto& [user, status] : users) {
        response.push_back(user.size());
        response.insert(response.end(), user.begin(), user.end());
        response.push_back(status);
    }

    ws.write(net::buffer(response));
}


void process_chat_message(const string& sender, const vector<uint8_t>& data, websocket::stream<tcp::socket>& ws) {
    if (data.size() < 3) return;

    uint8_t usernameLen = data[1];
    if (data.size() < 2 + usernameLen + 1) return;

    string recipient(data.begin() + 2, data.begin() + 2 + usernameLen);
    uint8_t messageLen = data[2 + usernameLen];
    if (data.size() < 3 + usernameLen + messageLen) return;

    string message(data.begin() + 3 + usernameLen, data.begin() + 3 + usernameLen + messageLen);

    cout << "💬 " << sender << " → " << recipient << ": " << message << endl;

    vector<uint8_t> response;
    response.push_back(55);  // Código de mensaje de respuesta
    response.push_back(sender.size());
    response.insert(response.end(), sender.begin(), sender.end());
    response.push_back(messageLen);
    response.insert(response.end(), message.begin(), message.end());

    if (recipient == "~") {
        // Chat general: enviar a todos los clientes conectados
        for (auto& [user, status] : users) {
            if (status == 1) {  // Solo usuarios activos
                ws.write(net::buffer(response));
            }
        }
    } else {
        // Mensaje privado: solo al destinatario
        if (users.find(recipient) != users.end() && users[recipient] == 1) {
            ws.write(net::buffer(response));
        } else {
            cerr << "⚠️ Usuario no disponible: " << recipient << endl;
        }
    }
}

void handle_message(const string& sender, const vector<uint8_t>& data, websocket::stream<tcp::socket>& ws) {
    if (data.empty()) return;

    uint8_t messageType = data[0];  // Código del mensaje

    switch (messageType) {
        case 1:  // Listar usuarios conectados
            send_users_list(sender, ws);
            break;
        case 4:  // Enviar mensaje a otro usuario o chat general
            process_chat_message(sender, data, ws);
            break;
        default:
            cerr << "⚠️ Mensaje no reconocido: " << (int)messageType << endl;
            break;
    }
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
            beast::flat_buffer buffer;
            ws.read(buffer);
            auto data = buffer.data();
            vector<uint8_t> message_data(boost::asio::buffer_cast<const uint8_t*>(data), 
                                         boost::asio::buffer_cast<const uint8_t*>(data) + boost::asio::buffer_size(data));

            if (!message_data.empty()) {
                handle_message(username, message_data, ws);
            }
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

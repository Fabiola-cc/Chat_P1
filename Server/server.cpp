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

// Añade esta estructura global
struct ClientSession {
    std::shared_ptr<websocket::stream<tcp::socket>> ws;
    int status;
};

// Modifica el diccionario de usuarios
std::unordered_map<std::string, ClientSession> clients;
std::mutex clients_mutex; // Mutex para proteger el acceso al diccionario

// Historial de mensajes
unordered_map<string, vector<pair<string, string>>> chatHistory;  // {usuario o "~" → [(remitente, mensaje)]}
mutex history_mutex;


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
    lock_guard<mutex> lock(clients_mutex);
    cout << "Usuarios disponibles [" << clients.size() << "]: ";
    for (const auto& [user, client] : clients) {
        cout << user << ", " << client.status << " |";
    }
    cout << endl;
}

void send_users_list(websocket::stream<tcp::socket>& ws) {
    lock_guard<mutex> lock(clients_mutex);

    vector<uint8_t> response;
    response.push_back(51);  // Código de mensaje (respuesta a listar usuarios)
    response.push_back(clients.size());  // Número de usuarios

    for (const auto& [user, client] : clients) {
        response.push_back(user.size());
        response.insert(response.end(), user.begin(), user.end());
        response.push_back(client.status);
    }

    ws.write(net::buffer(response));
}

void get_chat_history(const string& requester, const vector<uint8_t>& data, websocket::stream<tcp::socket>& ws) {
    if (data.size() < 2) return;

    uint8_t chatLen = data[1];
    if (data.size() < 2 + chatLen) return;

    string chatName(data.begin() + 2, data.begin() + 2 + chatLen);

    vector<uint8_t> response;
    response.push_back(56);  // Código de respuesta de historial
    {
        lock_guard<mutex> lock(history_mutex);

        uint8_t numMessages = chatHistory[chatName].size();
        response.push_back(numMessages);  // Número de mensajes

        for (const auto& [sender, msg] : chatHistory[chatName]) {
            response.push_back(sender.size());
            response.insert(response.end(), sender.begin(), sender.end());
            response.push_back(msg.size());
            response.insert(response.end(), msg.begin(), msg.end());
        }
    }

    ws.write(net::buffer(response));
}

void process_chat_message(const string& sender, const vector<uint8_t>& data) {
    if (data.size() < 3) return;

    uint8_t usernameLen = data[1];
    if (data.size() < 2 + usernameLen + 1) return;

    string recipient(data.begin() + 2, data.begin() + 2 + usernameLen);
    uint8_t messageLen = data[2 + usernameLen];
    if (data.size() < 3 + usernameLen + messageLen) return;

    string message(data.begin() + 3 + usernameLen, data.begin() + 3 + usernameLen + messageLen);

    cout << "💬 " << sender << " → " << recipient << ": " << message << endl;

    {
        lock_guard<mutex> lock(history_mutex);
        chatHistory[recipient].emplace_back(sender, message);  // Guardamos en el historial
    }

    vector<uint8_t> response;
    response.push_back(55);  // Código de mensaje recibido
    response.push_back(sender.size());
    response.insert(response.end(), sender.begin(), sender.end());
    response.push_back(messageLen);
    response.insert(response.end(), message.begin(), message.end());

    lock_guard<mutex> lock(clients_mutex);
    
    // Enviar el mensaje al remitente (para que lo vea también)
    if (clients.find(sender) != clients.end() && clients[sender].status == 1) {
        clients[sender].ws->write(net::buffer(response));
    }

    if (recipient == "~") {
        // Chat general: enviar a TODOS los demás clientes conectados
        for (auto& [user, client] : clients) {
            if (client.status == 1 && user != sender) {
                client.ws->write(net::buffer(response));
            }
        }
    } else {
        // Mensaje privado: enviar solo al destinatario
        if (clients.find(recipient) != clients.end() && clients[recipient].status == 1) {
            clients[recipient].ws->write(net::buffer(response));
        } else {
            cerr << "⚠️ Usuario no disponible: " << recipient << endl;
        }
    }
}
void handle_message(const string& sender, const vector<uint8_t>& data) {
    if (data.empty()) return;

    uint8_t messageType = data[0];  // Código del mensaje

    switch (messageType) {
        case 1:  // Listar usuarios conectados
            {
                lock_guard<mutex> lock(clients_mutex);
                if (clients.find(sender) != clients.end() && clients[sender].status == 1) {
                    send_users_list(*clients[sender].ws);
                }
            }
            break;
        case 4:  // Enviar mensaje a otro usuario o chat general
            process_chat_message(sender, data);
            break;
        case 5: // Cargar historial de mensajes
            {
                lock_guard<mutex> lock(clients_mutex);
                if (clients.find(sender) != clients.end() && clients[sender].status == 1) {
                    get_chat_history(sender, data, *clients[sender].ws);
                }
            }
            break;
        default:
            cerr << "⚠️ Mensaje no reconocido: " << (int)messageType << endl;
            break;
    }
}

void do_session(tcp::socket socket) {
    string username;
    // Crear un shared_ptr para el WebSocket
    auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
    
    try {
        beast::flat_buffer buffer;
        beast::http::request<beast::http::string_body> req;
        beast::http::read(ws->next_layer(), buffer, req);

        username = extract_username(req);

        bool shouldAccept = false;

        {
            lock_guard<mutex> lock(clients_mutex);
            if (clients.find(username) == clients.end() || clients[username].status == 0) {
                clients[username] = {ws, 1};
                cout << "✅ " << (clients.find(username) == clients.end() ? "Nuevo usuario conectado: " : "Usuario reconectado: ") << username << endl;
                shouldAccept = true;
            } else {
                cout << "⚠️ Intento de reconexión fallido para: " << username << endl;
                ws->close(websocket::close_code::normal);
                return;
            }
        }

        if (shouldAccept) {  
            ws->accept(req);
        }

        print_users();
        send_users_list(*ws);  

        while (true) {
            beast::flat_buffer buffer;
            ws->read(buffer);
            auto data = buffer.data();
            vector<uint8_t> message_data(boost::asio::buffer_cast<const uint8_t*>(data), 
                                       boost::asio::buffer_cast<const uint8_t*>(data) + boost::asio::buffer_size(data));

            if (!message_data.empty()) {
                handle_message(username, message_data);
            }
        }
    } catch (const std::exception& e) {
        cerr << "⚠️ Error en la sesión: " << e.what() << endl;
    }

    if (!username.empty()) {
        {
            lock_guard<mutex> lock(clients_mutex);
            if (clients.find(username) != clients.end()) {
                clients[username].status = 0;
            }
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

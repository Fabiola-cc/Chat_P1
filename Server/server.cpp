#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <string>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;
using namespace std;

// Añade esta estructura global
struct ClientSession {
    std::shared_ptr<websocket::stream<tcp::socket>> ws;
    int status;
};

std::unordered_map<std::string, ClientSession> clients;
std::mutex clients_mutex;

unordered_map<string, vector<pair<string, string>>> chatHistory;
mutex history_mutex;

string extract_username(const beast::http::request<beast::http::string_body>& req) {
    string target(req.target());
    size_t pos = target.find("?name=");

    if (pos != string::npos) {
        return target.substr(pos + 6);
    }
    return "Desconocido";
}

std::string get_status_string(int status) {
    switch (status) {
        case 0: return "Desconectado";
        case 1: return "Activo";
        case 2: return "Ocupado";
        case 3: return "Inactivo";
        default: return "Desconocido";
    }
}

void print_users() {
    std::lock_guard<std::mutex> lock(clients_mutex);
    std::cout << "Usuarios registrados [" << clients.size() << "]: ";
    for (const auto& [username, session] : clients) {
        std::cout << username << " (Estado: " << get_status_string(session.status)
                  << ", WebSocket: " << (session.ws && session.ws->is_open() ? "Abierto" : "Cerrado") 
                  << ") | ";
    }
    std::cout << std::endl;
}


void send_users_list(websocket::stream<tcp::socket>& ws) {
    lock_guard<mutex> lock(clients_mutex);

    vector<uint8_t> response;
    response.push_back(51);
    response.push_back(clients.size());

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
    response.push_back(56);

    {
        lock_guard<mutex> lock(history_mutex);

        uint8_t numMessages = chatHistory[chatName].size();
        response.push_back(numMessages);

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
        chatHistory[recipient].emplace_back(sender, message);
    }

    vector<uint8_t> response;
    response.push_back(55);
    response.push_back(sender.size());
    response.insert(response.end(), sender.begin(), sender.end());
    response.push_back(messageLen);
    response.insert(response.end(), message.begin(), message.end());

    lock_guard<mutex> lock(clients_mutex);
    
    if (clients.find(sender) != clients.end() && clients[sender].status == 1) {
        clients[sender].ws->write(net::buffer(response));
    }

    if (recipient == "~") {
        for (auto& [user, client] : clients) {
            if (client.status == 1 && user != sender) {
                client.ws->write(net::buffer(response));
            }
        }
    } else {
        if (clients.find(recipient) != clients.end() && clients[recipient].status == 1) {
            clients[recipient].ws->write(net::buffer(response));
        } else {
            cerr << "⚠️ Usuario no disponible: " << recipient << endl;
        }
    }
}

void handle_message(const string& sender, const vector<uint8_t>& data) {
    if (data.empty()) return;

    uint8_t messageType = data[0];

    switch (messageType) {
        case 1:
            {
                lock_guard<mutex> lock(clients_mutex);
                if (clients.find(sender) != clients.end() && clients[sender].status == 1) {
                    send_users_list(*clients[sender].ws);
                }
            }
            break;
        case 4:
            process_chat_message(sender, data);
            break;
        case 5:
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

bool verificarEncabezadosWebSocket(const http::request<http::string_body>& req, tcp::socket& socket) {
    // Verificar encabezados WebSocket
    auto connection = req[http::field::connection];
    auto upgrade = req[http::field::upgrade];
    auto key = req["Sec-WebSocket-Key"];
    auto version = req["Sec-WebSocket-Version"];

    if (connection != "Upgrade" || upgrade != "websocket" || key.empty() || version != "13") {
        // Si los encabezados son incorrectos, enviar una respuesta de error
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.body() = "Encabezados WebSocket incorrectos.";
        http::write(socket, res);
        std::cout << "Encabezados WebSocket incorrectos. " << std::endl;
        return false; // Indica que la verificación falló
    }

    return true; // Indica que la verificación fue exitosa
}

void do_session(net::ip::tcp::socket socket) {
    std::string username;
    auto ws = std::make_shared<websocket::stream<net::ip::tcp::socket>>(std::move(socket));
    bool connectionAccepted = false;  // Indica si la conexión fue aceptada

    try {
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(ws->next_layer(), buffer, req);

        if (!verificarEncabezadosWebSocket(req, socket)) {
            return;
        }

        // Extraer el nombre de usuario
        username = extract_username(req);

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            if (username == "~"){
                http::response<http::string_body> res{http::status::bad_request, req.version()};
                res.body() = "No se acepta el nombre de usuario: ~";
                http::write(socket, res);
                std::cout << "No se acepta el nombre de usuario: " << username << std::endl;
                return; 
            }
            if (clients.find(username) == clients.end()) {
                // Caso 1: Usuario completamente nuevo
                clients[username] = {ws, 1};  // Estado: Activo
                std::cout << "✅ Nuevo usuario conectado: " << username << std::endl;
                connectionAccepted = true;
            } else if (clients[username].status == 0) {
                // Caso 2: Usuario estaba desconectado y se reconecta
                clients[username] = {ws, 1};  // Estado: Activo
                std::cout << "🔄 Usuario reconectado: " << username << std::endl;
                connectionAccepted = true;
            } else {
                // Usuario ya está conectado, rechazar sin cerrar su sesión
                http::response<http::string_body> res{http::status::bad_request, req.version()};
                res.body() = "Usuario ya se encuentra registrado";
                http::write(socket, res);
                std::cout << "❌ Error: Usuario '" << username << "' ya está conectado" << std::endl;
                return;  // Salir sin cerrar la sesión del usuario original
            }
        }

        // Aceptar WebSocket solo si fue permitido
        if (connectionAccepted) {
            ws->accept(req);
            std::cout << "🔗 Cliente conectado\n";
            print_users();
            send_users_list(*ws);
        }

        // Mantener la sesión activa
        while (connectionAccepted) {
            if (!ws->is_open()) {
                std::cout << "❌ Conexión WebSocket cerrada por el cliente: " << username << std::endl;
                break;
            }

            beast::flat_buffer buffer;
            ws->read(buffer);

            auto data = buffer.data();
            std::vector<uint8_t> message_data(boost::asio::buffer_cast<const uint8_t*>(data),
                                              boost::asio::buffer_cast<const uint8_t*>(data) + boost::asio::buffer_size(data));

            if (!message_data.empty()) {
                handle_message(username, message_data);
            }
        }
    } catch (const boost::system::system_error& e) {
        std::cerr << "❌ Error de sistema: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Excepción: " << e.what() << std::endl;
    }

    // Cerrar sesión solo si la conexión fue aceptada
    if (connectionAccepted) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        if (clients.find(username) != clients.end()) {
            clients[username].status = 0;
            std::cout << "❌ Usuario desconectado: " << username << " (Estado: Desconectado)" << std::endl;
        }
    }

    print_users();
}


int main() {
    try {
        net::io_context ioc;
        tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 8080));
        cout << "🌐 Servidor WebSocket en el puerto 8080...\n";

        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
           

            // Manejar la sesión en un hilo separado
            thread{do_session, move(socket)}.detach();
        }

    } catch (const exception& e) {
        cerr << "❌ Error: " << e.what() << endl;
    }

    return 0;
}

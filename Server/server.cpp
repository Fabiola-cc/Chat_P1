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
        res.body() = "❌ Encabezados WebSocket incorrectos.";
        http::write(socket, res);
        std::cout << "❌ Encabezados WebSocket incorrectos. " << std::endl;
        return false; // Indica que la verificación falló
    }

    return true; // Indica que la verificación fue exitosa
}

void do_session(net::ip::tcp::socket socket) {
    std::string username;
    auto ws = std::make_shared<websocket::stream<net::ip::tcp::socket>>(std::move(socket));


    try {
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(ws->next_layer(), buffer, req);

        if (!verificarEncabezadosWebSocket(req, socket)) {
            return;
        }

        // Extraer el nombre de usuario
        username = extract_username(req);
        bool shouldAccept = false;

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            if (clients.find(username) == clients.end() || clients[username].status == 0) {
                clients[username] = {ws, 1};
                std::cout << "✅ Nuevo usuario conectado: " << username << std::endl;
                shouldAccept = true;
            } else {
                http::response<http::string_body> res{http::status::bad_request, req.version()};
                res.body() = "Usuario ya se encuentra registrado";
                http::write(socket, res);
                ws->close(websocket::close_code::normal);  // Cerrar la conexión WebSocket
                std::cout << "❌ Error: Usuario ya se encuentra registrado " << std::endl;
                return;
            }
        }

        // Aceptar la conexión WebSocket si es válido
        if (shouldAccept) {
            ws->accept(req);
            cout << "🔗 Cliente conectado\n";
        }

        // Enviar lista de usuarios y mantener la sesión activa
        print_users();
        send_users_list(*ws);

        while (true) {
            if (!ws->is_open()) {
                std::cout << "❌ Conexión WebSocket cerrada. Terminando sesión." << std::endl;
                break;
            }

            beast::flat_buffer buffer;
            ws->read(buffer);  // Leer mensaje del cliente

            auto data = buffer.data();
            std::vector<uint8_t> message_data(boost::asio::buffer_cast<const uint8_t*>(data),
                                              boost::asio::buffer_cast<const uint8_t*>(data) + boost::asio::buffer_size(data));

            if (!message_data.empty()) {
                handle_message(username, message_data);  // Procesar el mensaje
            }
        }
    } catch (const boost::system::system_error& e) {
        // Manejar errores específicos de Boost
        std::cerr << "❌ Error de sistema: " << e.what() << std::endl;
        if (ws->is_open()) {
            ws->close(websocket::close_code::normal);  // Cerrar la conexión WebSocket en caso de error
        }
    } catch (const std::exception& e) {
        // Manejar otros errores generales
        std::cerr << "❌ Excepción: " << e.what() << std::endl;
        if (ws->is_open()) {
            ws->close(websocket::close_code::normal);  // Cerrar la conexión WebSocket en caso de error
        }
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
           

            // Manejar la sesión en un hilo separado
            thread{do_session, move(socket)}.detach();
        }

    } catch (const exception& e) {
        cerr << "❌ Error: " << e.what() << endl;
    }

    return 0;
}

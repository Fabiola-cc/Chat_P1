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

// Definiendo alias para espacios de nombres comúnmente utilizados
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;
using namespace std;

/**
 * Estructura que representa una sesión de cliente.
 * Mantiene el socket WebSocket, el estado del usuario y su dirección IP.
 */
struct ClientSession {
    std::shared_ptr<websocket::stream<tcp::socket>> ws;  // Socket WebSocket para la comunicación
    int status;                                          // Estado del usuario (0:Desconectado, 1:Activo, 2:Ocupado, 3:Inactivo)
    std::string ipAddress;                               // Dirección IP del cliente
};

// Mapa que almacena todas las sesiones de clientes conectados, indexado por nombre de usuario
std::unordered_map<std::string, ClientSession> clients;
// Mutex para proteger el acceso concurrente al mapa de clientes
std::mutex clients_mutex;

// Mapa que almacena el historial de chat
// La clave es un ID del chat, 
unordered_map<string, vector<pair<string, string>>> chatHistory; 
// Mutex para proteger el acceso concurrente al historial
mutex history_mutex;

/**
 * Extrae el nombre de usuario de la URL de la solicitud HTTP.
 * Busca el parámetro "?name=" en la URL.
 * 
 * @param target Solicitud HTTP recibida
 * @return El nombre de usuario extraído o "Desconocido" si no se encuentra
 */
std::string extract_username(const std::string& target) {
    size_t pos = target.find("?name=");

    if (pos != std::string::npos) {
        return target.substr(pos + 6);
    }
    return "Desconocido";
}


/**
 * Convierte el código numérico de estado a una cadena descriptiva.
 * 
 * @param status Código de estado (0-3)
 * @return Descripción textual del estado
 */
std::string get_status_string(int status) {
    switch (status) {
        case 0: return "Desconectado";
        case 1: return "Activo";
        case 2: return "Ocupado";
        case 3: return "Inactivo";
        default: return "Desconocido";
    }
}

/**
 * Imprime en la consola la lista de usuarios registrados y su estado actual.
 * Útil para depuración y monitoreo del servidor.
 */
void print_users() {
    std::lock_guard<std::mutex> lock(clients_mutex);
    cout << "Usuarios registrados [" << clients.size() << "]: ";
    for (const auto& [username, session] : clients) {
        cout << username << " (Estado: " << get_status_string(session.status)
                  << ", WebSocket: " << (session.ws && session.ws->is_open() ? "Abierto" : "Cerrado") 
                  << ") | ";
    }
    cout << endl;
}

/** 
 * Genere una clave única para cada conversación
 * 
 * @param user1 uno de los usuarios en la conversación
 * @param user2 uno de los usuarios en la conversación
*/
string get_chat_id(const string& user1, const string& user2) {
    if (user1 < user2) {
        return user1 + "-" + user2;  // Orden lexicográfico
    } else {
        return user2 + "-" + user1;
    }
}


/**
 * Envía la lista de usuarios conectados al cliente solicitante.
 * Formato del mensaje: [51, número_usuarios, [longitud_nombre, nombre, estado], ...]
 * 
 * @param ws Socket WebSocket del cliente al que enviar la información
 */
void send_users_list_unlocked(websocket::stream<tcp::socket>& ws) {
    // Same as send_users_list but without locking the mutex
    // The caller must ensure the mutex is already locked
    
    vector<unsigned char> response;
    response.push_back(static_cast<unsigned char>(51));  // Code 51: User list
    response.push_back(static_cast<unsigned char>(clients.size()));  // Number of users
    
    // Add each user's information
    for (const auto& [user, client] : clients) {
        response.push_back(static_cast<unsigned char>(user.size()));
        response.insert(response.end(), user.begin(), user.end());
        response.push_back(static_cast<unsigned char>(client.status));
    }
    
    try {
        cout << "📜 Sending list of " << clients.size() << " users..." << endl;
        ws.write(net::buffer(response));
        cout << "📜📢 Response sent successfully" << endl;
    } 
    catch (const std::exception& e) {
        cerr << "❌ Error sending user list: " << e.what() << endl;
    }
}

// Keep the original function but make it use the unlocked version:
void send_users_list(websocket::stream<tcp::socket>& ws) {
    lock_guard<mutex> lock(clients_mutex);
    send_users_list_unlocked(ws);
}




/**
 * Procesa la solicitud de cambio de estado de un usuario.
 * Formato del mensaje: [3, longitud_nombre, nombre, nuevo_estado]
 * Actualiza el estado del usuario y notifica a todos los clientes.
 * 
 * @param data Buffer con el mensaje recibido
 */
 void change_state(const std::vector<unsigned char>& data) {
    std::vector<unsigned char> message;

    // Validar longitud mínima del mensaje
    if (data.size() < 3) {
        cerr << "❌ Error: Mensaje demasiado corto para cambiar estado." << endl;
        return;
    }

    unsigned char username_len = data[1];

    // Validar que el mensaje contiene el nombre completo
    if (username_len + 2 > data.size()) {
        cerr << "❌ Error: El tamaño del nombre de usuario es incorrecto." << endl;
        return;
    }

    // Obtener el nombre de usuario
    std::string received_username(reinterpret_cast<const char*>(&data[2]), username_len);

    // El último byte es el nuevo estado
    unsigned char new_status = data[2 + username_len];
    if (!(new_status >= 0 && new_status <= 3)) {
        message.push_back(50);                  // Código 50: Error
        message.push_back(2);                   // Estado inválido
        
        cerr << "❌ Error: Estado del usuario inválido." << endl;
    }

    // Cambiar el estado del usuario
    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = clients.find(received_username);
    if (it != clients.end()) {
        it->second.status = new_status;
        cout << "📢 El usuario " << received_username << " cambió su estado a " 
                  << static_cast<int>(new_status) << endl;

        // Construir el mensaje para el cambio de estado
        message.push_back(54);  // Tipo de mensaje 54: Notificación de cambio de estado
        message.push_back(static_cast<unsigned char>(received_username.size()));  // Longitud del nombre de usuario
        message.insert(message.end(), received_username.begin(), received_username.end());  // Nombre de usuario
        message.push_back(new_status);  // Nuevo estado

        // Enviar el mensaje a todos los clientes conectados
        for (auto& client : clients) {
            if (client.second.ws->is_open()) {
                client.second.ws->write(net::buffer(message));  // Enviar el mensaje
            }
        }
        cout << "🫥📢 Respuesta enviada" << endl;
    } else {
        cerr << "❌ Error: Usuario no encontrado." << endl;
    }
}


/**
 * Envía el historial de chat al cliente solicitante.
 * Formato solicitud: [5, longitud_nombre_chat, nombre_chat]
 * Formato respuesta: [56, num_mensajes, [longitud_emisor, emisor, longitud_mensaje, mensaje], ...]
 * 
 * @param requester Nombre del usuario que solicita el historial
 * @param data Buffer con el mensaje recibido
 * @param ws Socket WebSocket del cliente
 */
 void get_chat_history(const string& requester, const vector<unsigned char>& data, websocket::stream<tcp::socket>& ws) {
    // Validar longitud mínima del mensaje
    if (data.size() < 2) return;

    unsigned char chatLen = static_cast<unsigned char>(data[1]);
    if (data.size() < 2 + chatLen) return;

    // Extraer nombre del chat solicitado
    string chatName(data.begin() + 2, data.begin() + 2 + chatLen);
    
    // Generar la clave del chat
    string chat_id = chatName != "~" ? get_chat_id(requester, chatName) : chatName;

    // Construir respuesta
    vector<unsigned char> response;
    response.push_back(static_cast<unsigned char>(56));  // Código 56: Historial de chat

    {
        lock_guard<mutex> lock(history_mutex);

        unsigned char numMessages = static_cast<unsigned char>(chatHistory[chat_id].size());
        response.push_back(numMessages);  // Número de mensajes

        // Agregar cada mensaje del historial
        for (const auto& [sender, msg] : chatHistory[chat_id]) {
            response.push_back(static_cast<unsigned char>(sender.size()));  // Longitud del emisor
            response.insert(response.end(), sender.begin(), sender.end());  // Nombre del emisor
            response.push_back(static_cast<unsigned char>(msg.size()));     // Longitud del mensaje
            response.insert(response.end(), msg.begin(), msg.end());        // Contenido del mensaje
        }
    }

    // Enviar respuesta
    ws.write(net::buffer(response));
    cout << "🕘📢 Respuesta con historial enviada " << chat_id << endl;
}

/**
 * Procesa un mensaje de chat y lo reenvía al destinatario.
 * Formato del mensaje: [4, longitud_destinatario, destinatario, longitud_mensaje, mensaje]
 * También almacena el mensaje en el historial de chat.
 * 
 * @param sender Nombre del usuario que envía el mensaje
 * @param data Buffer con el mensaje recibido
 */
 void process_chat_message(const string& sender, const vector<unsigned char>& data) {
    // Validar longitud mínima del mensaje
    if (data.size() < 3) return;

    unsigned char usernameLen = static_cast<unsigned char>(data[1]);
    if (data.size() < 2 + usernameLen + 1) return;

    // Extraer destinatario
    string recipient(data.begin() + 2, data.begin() + 2 + usernameLen);
    
    unsigned char messageLen = static_cast<unsigned char>(data[2 + usernameLen]);
    if (data.size() < 3 + usernameLen + messageLen) return;

    // Extraer contenido del mensaje
    if (messageLen == 0) {
        vector<unsigned char> response;
        response.push_back(50); // ERROR
        response.push_back(3);  // Mensaje vacío
    }
    
    string message(data.begin() + 3 + usernameLen, data.begin() + 3 + usernameLen + messageLen);

    cout << "💬 " << sender << " → " << recipient << ": " << message << endl;

    // Guardar en historial
    {
        lock_guard<mutex> lock(history_mutex);
        // Usa el id del chat, solamente el chat general usa su nombre como id
        string chat_id = recipient != "~" ? get_chat_id(sender, recipient) : recipient;
        chatHistory[chat_id].emplace_back(sender, message);
    }    

    // Trabajar con chat general
    string New_sender = sender;
    if (recipient == "~"){
        New_sender = recipient;
        message = sender + ": " + message;
        messageLen = static_cast<unsigned char>(message.size());
    }

    // Preparar mensaje para reenvío
    vector<unsigned char> response;
    response.push_back(static_cast<unsigned char>(55));  // Código 55: Mensaje de chat
    response.push_back(static_cast<unsigned char>(New_sender.size()));
    response.insert(response.end(), New_sender.begin(), New_sender.end());
    response.push_back(messageLen);
    response.insert(response.end(), message.begin(), message.end());

    lock_guard<mutex> lock(clients_mutex);
    
    // Enviar copia al emisor
    if (clients.find(sender) != clients.end() && clients[sender].status == 1) {
        clients[sender].ws->write(net::buffer(response));
        cout << "💬📢 Mensaje enviado al emisor" << endl;
    }

    // Si el destinatario es "~", es un mensaje para todos (broadcast)
    if (recipient == "~") {
        for (auto& [user, client] : clients) {
            if (client.status == 1 && user != sender) {
                client.ws->write(net::buffer(response));
            }
        }
        cout << "💬📢 Mensaje enviado al todos" << endl;
    } else {
        // Enviar al destinatario específico
        if (clients.find(recipient) != clients.end() && clients[recipient].status != 0) {
            clients[recipient].ws->write(net::buffer(response));
            cout << "💬📢 Mensaje enviado al receptor" << endl;
        } else {
            vector<unsigned char> error;
            if (clients.find(recipient) == clients.end())
            {
                error.push_back(50); // ERROR
                error.push_back(1);  // usuario inexistente
                clients[sender].ws->write(net::buffer(error));
            }
            
            int actualStatus = clients[recipient].status;
            if (actualStatus == 0) {
                error.push_back(50); // ERROR
                error.push_back(4);  // usuario con estatus desconectado
                clients[sender].ws->write(net::buffer(error));
            }
            
            cerr << "⚠️ Usuario no disponible: " << recipient << endl;
        }
    }
}


/**
 * Envía información sobre un usuario específico al solicitante.
 * Formato solicitud: [2, longitud_nombre, nombre]
 * Formato respuesta: [52, exito, longitud_nombre, nombre, estado]
 * 
 * @param requester Nombre del usuario que solicita la información
 * @param data Buffer con el mensaje recibido
 */
 void send_info(const string& requester, const vector<unsigned char>& data) {
    // Validación de datos entrantes
    if (data.size() < 2) {
        cerr << "❌ Error: Datos insuficientes para obtener información de usuario." << endl;
        return;
    }

    unsigned char usernameLen = static_cast<unsigned char>(data[1]);
    if (data.size() < 2 + usernameLen) {
        cerr << "❌ Error: Longitud del nombre de usuario incorrecta." << endl;
        return;
    }

    // Extracción del nombre de usuario solicitado
    string targetUsername(data.begin() + 2, data.begin() + 2 + usernameLen);
    cout << "🔍 " << requester << " solicita información de: " << targetUsername << endl;

    // Creación del paquete de respuesta
    vector<unsigned char> response;
    response.push_back(static_cast<unsigned char>(52));  // Tipo 52: Información de usuario

    // Búsqueda de información del usuario
    lock_guard<mutex> lock(clients_mutex);
    auto it = clients.find(targetUsername);
    
    if (it != clients.end()) {
        // Usuario encontrado - incluir información
       // response.push_back(static_cast<unsigned char>(1));  // Indicador de éxito
        response.push_back(usernameLen);
        response.insert(response.end(), targetUsername.begin(), targetUsername.end());
        response.push_back(it->second.status);
        
        cout << "ℹ️ Información de usuario " << targetUsername << " enviada a " << requester << endl;
    } else {
        response.clear();  // Limpiar la respuesta anterior
        response.push_back(static_cast<unsigned char>(50));  // Código 50: Error
        response.push_back(static_cast<unsigned char>(1));   // Usuario no existente

        // Usuario no encontrado
        response.push_back(static_cast<unsigned char>(0));   // Indicador de fallo
        cout << "⚠️ Usuario " << targetUsername << " no encontrado" << endl;
    }

    // Envío de respuesta al solicitante
    auto requester_it = clients.find(requester);
    if (requester_it != clients.end() && requester_it->second.ws->is_open()) {
        requester_it->second.ws->write(net::buffer(response));
        cout << "ℹ️📢 Respuesta enviada a " << requester << endl;
    }
}


/**
 * Notifica a todos los usuarios conectados que hay un nuevo usuario en línea.
 * Formato mensaje: [53, longitud_nombre, nombre, estado]
 * 
 * @param username Nombre del nuevo usuario
 */
 void broadcast_new_user(const std::string& username) {
    std::vector<unsigned char> message;

    message.push_back(static_cast<unsigned char>(53));  // Tipo 53: Nuevo usuario
    message.push_back(static_cast<unsigned char>(username.size()));  // Longitud del nombre

    // Agregar el nombre de usuario
    message.insert(message.end(), username.begin(), username.end());

    message.push_back(static_cast<unsigned char>(1));  // Estado inicial: Activo

    // Enviar a todos los usuarios activos
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& [user, client] : clients) {
        if (client.status == 1 && client.ws && client.ws->is_open()) {
            try {
                client.ws->write(net::buffer(message));
            } catch (const boost::system::system_error& e) {
                cerr << "⚠️ No se pudo enviar mensaje a " << user << ": " << e.what() << endl;
            }
        }
    }
    cout << "😁📢 Respuesta enviada a todos los usuarios"<< endl;
}

/**
 * Procesa los mensajes recibidos de los clientes según su tipo.
 * Cada tipo de mensaje tiene un código específico:
 * 1: Solicitud de lista de usuarios
 * 2: Solicitud de información de usuario
 * 3: Cambio de estado
 * 4: Mensaje de chat
 * 5: Solicitud de historial de chat
 * 
 * @param sender Nombre del usuario que envía el mensaje
 * @param data Buffer con el mensaje recibido
 */
 void handle_message(const string& sender, const vector<unsigned char>& data) {
    if (data.empty()) return;

    unsigned char messageType = data[0];

    switch (messageType) {
        case 1:  // Solicitud de lista de usuarios
            {
                cout << "📜 [" << std::this_thread::get_id() << "] User list request from: " << sender << endl;
                lock_guard<mutex> lock(clients_mutex);
                
                auto it = clients.find(sender);
                if (it != clients.end() && it->second.ws->is_open()) {
                    send_users_list_unlocked(*it->second.ws);
                } else {
                    cout << "📜🔴 Cannot send user list (user not found or disconnected)" << endl;
                }
            }
            break;
        case 2:  // Solicitud de información de usuario
            cout << "ℹ️ [" << std::this_thread::get_id() << "] Solicitud de info de usuario de: " << sender << endl;
            send_info(sender, data);
            break;
        case 3:  // Cambio de estado
            cout << "🫥 [" << std::this_thread::get_id() << "] Cambio de estado solicitado por: " << sender << endl;
            change_state(data);
            break;
        case 4:  // Mensaje de chat
            cout << "💬 [" << std::this_thread::get_id() << "] Mensaje de chat recibido de: " << sender << endl;
            process_chat_message(sender, data);
            break;
        case 5:  // Solicitud de historial de chat
            {
                cout << "🕘 [" << std::this_thread::get_id() << "] Solicitud de historial de: " << sender << endl;
                lock_guard<mutex> lock(clients_mutex);
                if (clients.find(sender) != clients.end() && clients[sender].status == 1) {
                    get_chat_history(sender, data, *clients[sender].ws);
                } else {
                    cout << "🕘🔴 [" << std::this_thread::get_id() << "] No se pudo recuperar historial de chat (usuario no encontrado o no disponible)." << endl;
                }
            }
            cout << "🔓 Mutex liberado" << endl;
            break;
        default:
            cerr << "⚠️ [" << std::this_thread::get_id() << "] Mensaje no reconocido: " << (int)messageType << endl;
            break;
    }    
}



/**
 * Verifica que la solicitud HTTP contenga los encabezados necesarios para establecer
 * una conexión WebSocket válida.
 * 
 * @param req Solicitud HTTP recibida
 * @param socket Socket TCP para responder en caso de error
 * @return true si los encabezados son válidos, false en caso contrario
 */
bool verificarEncabezadosWebSocket(const http::request<http::string_body>& req, tcp::socket& socket) {
    // Verificar encabezados WebSocket - comparación exacta según el protocolo
    auto connection = req[http::field::connection];
    auto upgrade = req[http::field::upgrade];
    auto key = req["Sec-WebSocket-Key"];
    auto version = req["Sec-WebSocket-Version"];

    if (boost::beast::iequals(connection, "Upgrade") && 
        boost::beast::iequals(upgrade, "websocket") && 
        !key.empty() && 
        version == "13") {
        return true; // Encabezados correctos
    }
    
    // Si los encabezados son incorrectos, enviar una respuesta de error
    http::response<http::string_body> res{http::status::bad_request, req.version()};
    res.set(http::field::server, "WebSocketServer");
    res.set(http::field::content_type, "text/plain");
    res.body() = "Encabezados WebSocket incorrectos.";
    res.prepare_payload();
    http::write(socket, res);
    cout << "❌ Encabezados WebSocket incorrectos." << endl;
    return false;
}

/**
 * Maneja una sesión completa para un cliente conectado.
 * Gestiona la conexión inicial, el intercambio de mensajes y el cierre.
 * 
 * @param socket Socket TCP establecido con el cliente
 */
 void do_session(net::ip::tcp::socket socket) {
    beast::flat_buffer buffer;
    http::request<http::string_body> req;
    std::string username;
    bool connectionAccepted = false;
    bool newRegister = false;
    

    try {
        // Leer la solicitud HTTP inicial
        http::read(socket, buffer, req);

        std::string connHdr = std::string(req[http::field::connection]);
        std::string upgHdr = std::string(req[http::field::upgrade]);
        std::string target = std::string(req.target());
        username = extract_username(target); 

        // Verificar si la solicitud contiene los encabezados correctos para WebSocket
        if (connHdr.find("Upgrade") == std::string::npos || upgHdr.find("websocket") == std::string::npos) {
            http::response<http::string_body> res{http::status::ok, req.version()};

            // Validación del nombre de usuario
            if (username.empty() || username == "~") {
                cout << "🧐 Nombre de usuario no permitido: " << username << "\n";
                http::response<http::string_body> res{http::status::bad_request, req.version()};
                res.body() = "Nombre de usuario no permitido.";
                http::write(socket, res);
                return;
            }

            // Comprobación de si el usuario ya está conectado
            std::lock_guard<std::mutex> lock(clients_mutex);
            auto it = clients.find(username);
            if (it != clients.end() && it->second.status != 0) {
                cout << "😶‍🌫️ Usuario ya está conectado: " << username << "\n";
                http::response<http::string_body> res{http::status::bad_request, req.version()};
                res.body() = "Usuario ya está conectado.";
                http::write(socket, res);
                return;
            }
            res.prepare_payload();
            http::write(socket, res);
            return;
        }

        auto ws = std::make_shared<websocket::stream<net::ip::tcp::socket>>(std::move(socket));
        std::string clientIP = ws->next_layer().remote_endpoint().address().to_string();

        if (clients.find(username) == clients.end()) {
            // Caso 1: Usuario completamente nuevo
            clients[username] = {ws, 1, clientIP};  // Estado: Activo
            cout << "✅ Nuevo usuario conectado: " << username<< " desde " << clientIP  << endl;
            newRegister = true;
            connectionAccepted = true;
        } else if (clients[username].status == 0) {
            // Caso 2: Usuario estaba desconectado y se reconecta
            clients[username] = {ws, 1};  // Estado: Activo
            cout << "🔄 Usuario reconectado: " << username << " desde " << clientIP << endl;
            connectionAccepted = true;

            //Notificar el cambio de estado a activo
            std::vector<unsigned char> message;
            // Construir el mensaje para el cambio de estado
            message.push_back(54);  // Tipo de mensaje 54: Notificación de cambio de estado
            message.push_back(static_cast<unsigned char>(username.size()));  // Longitud del nombre de usuario
            message.insert(message.end(), username.begin(), username.end());  // Nombre de usuario
            message.push_back(1);  // Nuevo estado

            //NOTIFICAR A TODOS
            for (auto& [user, client] : clients) {
                if (user != username && client.ws && client.ws->is_open()) {
                    try {
                        client.ws->write(net::buffer(message));
                    } catch (const std::exception& e) {
                        cerr << "⚠️ Error al notificar desconexión a " << user << ": " << e.what() << endl;
                    }
                }
            }
        }

        if (connectionAccepted) {
            // Aceptar la conexión WebSocket
            ws->accept(req);
            cout << "🔗 Cliente conectado\n";
            print_users();
            if (newRegister){
                broadcast_new_user(username);
            }
            
        }

        // Bucle principal de la sesión
        while (connectionAccepted) {
            if (!ws->is_open()) {
                cout << "❌ Conexión WebSocket cerrada por el cliente: " << username << endl;
                break;
            }

            beast::flat_buffer buffer;
            ws->read(buffer);  // Leer datos del cliente WebSocket

            // Convertir los datos recibidos a un vector de bytes
            auto data = buffer.data();
            std::vector<unsigned char> message_data(boost::asio::buffer_cast<const unsigned char*>(data),
                                                    boost::asio::buffer_cast<const unsigned char*>(data) + boost::asio::buffer_size(data));

            if (!message_data.empty()) {
                cout<<"👀 Mensaje Recibido"<<endl;
                handle_message(username, message_data);  // Procesar el mensaje
            }
        }
    } catch (const boost::system::system_error& e) {
        if (e.code() == boost::beast::websocket::error::closed) {
            cout << "👋 Conexión cerrada limpiamente por " << username << endl;
        } else {
            cerr << "❌ Error de sistema: " << e.what() << endl;
        }
    } catch (const std::exception& e) {
        cerr << "❌ Excepción: " << e.what() << endl;
    }

    // Marcar usuario como desconectado y notificar a los demás
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        if (clients.find(username) != clients.end()) {
            clients[username].status = 0;  // Estado: Desconectado
            
            // Notificar a todos los usuarios del cambio de estado
            std::vector<unsigned char> stateChangeMsg;
            stateChangeMsg.push_back(54);  // Tipo 54: Cambio de estado
            stateChangeMsg.push_back(username.size());
            stateChangeMsg.insert(stateChangeMsg.end(), username.begin(), username.end());
            stateChangeMsg.push_back(0);  // Estado: Desconectado
            
            for (auto& [user, client] : clients) {
                if (user != username && client.ws && client.ws->is_open()) {
                    try {
                        client.ws->write(net::buffer(stateChangeMsg));
                    } catch (const std::exception& e) {
                        cerr << "⚠️ Error al notificar desconexión a " << user << ": " << e.what() << endl;
                    }
                }
            }
            
            cout << "👋 Usuario desconectado: " << username << endl;
        }
    }

    print_users();
}


/**
 * Función principal del programa.
 * Inicia el servidor WebSocket en el puerto 8080 y acepta conexiones entrantes.
 */
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
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <cstring>
#include <thread>
#include <mutex>
#include <vector>
#include <set>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>

#define PORT 12345
#define BUFFER_SIZE 1024

struct Client {
    int socket;
    std::string username;
    std::set<std::string> groups;
};

class ChatServer {
private:
    std::unordered_map<std::string, std::string> users;  // username -> password
    std::unordered_map<int, Client> active_clients;      // socket -> Client
    std::unordered_map<std::string, std::set<int>> groups;  // group name -> set of client sockets
    std::mutex clients_mutex;
    std::mutex groups_mutex;

    void load_users(const std::string& filename) {
        std::ifstream file(filename);
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string username, password;
            if (std::getline(iss, username, ':') && std::getline(iss, password)) {
                users[username] = password;
            }
        }
    }

    void broadcast_message(const std::string& sender, const std::string& message, int exclude_socket = -1) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        std::string full_message = sender + ": " + message + "\n";
        for (const auto& client : active_clients) {
            if (client.first != exclude_socket) {
                send(client.first, full_message.c_str(), full_message.length(), 0);
            }
        }
    }

    void send_private_message(const std::string& sender, const std::string& recipient, const std::string& message) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (const auto& client : active_clients) {
            if (client.second.username == recipient) {
                std::string full_message = "[PM from " + sender + "]: " + message + "\n";
                send(client.first, full_message.c_str(), full_message.length(), 0);
                return;
            }
        }
        // If recipient not found, send error message back to sender
        std::string error_message = "Error: User " + recipient + " not found\n";
        for (const auto& client : active_clients) {
            if (client.second.username == sender) {
                send(client.first, error_message.c_str(), error_message.length(), 0);
                return;
            }
        }
    }

    void send_group_message(const std::string& sender, const std::string& group_name, const std::string& message) {
        std::lock_guard<std::mutex> lock(groups_mutex);
        if (groups.find(group_name) == groups.end()) {
            std::string error_message = "Error: Group " + group_name + " does not exist\n";
            for (const auto& client : active_clients) {
                if (client.second.username == sender) {
                    send(client.first, error_message.c_str(), error_message.length(), 0);
                    return;
                }
            }
            return;
        }

        std::string full_message = "[" + group_name + "] " + sender + ": " + message + "\n";
        for (int client_socket : groups[group_name]) {
            send(client_socket, full_message.c_str(), full_message.length(), 0);
        }
    }

    void create_group(const std::string& creator, const std::string& group_name) {
        std::lock_guard<std::mutex> lock(groups_mutex);
        if (groups.find(group_name) != groups.end()) {
            std::string error_message = "Error: Group " + group_name + " already exists\n";
            for (const auto& client : active_clients) {
                if (client.second.username == creator) {
                    send(client.first, error_message.c_str(), error_message.length(), 0);
                    return;
                }
            }
            return;
        }

        groups[group_name] = std::set<int>();
        std::string success_message = "Group " + group_name + " created successfully\n";
        for (const auto& client : active_clients) {
            if (client.second.username == creator) {
                send(client.first, success_message.c_str(), success_message.length(), 0);
                join_group(creator, group_name);
                return;
            }
        }
    }

    void join_group(const std::string& username, const std::string& group_name) {
        std::lock_guard<std::mutex> lock(groups_mutex);
        if (groups.find(group_name) == groups.end()) {
            std::string error_message = "Error: Group " + group_name + " does not exist\n";
            for (const auto& client : active_clients) {
                if (client.second.username == username) {
                    send(client.first, error_message.c_str(), error_message.length(), 0);
                    return;
                }
            }
            return;
        }

        for (auto& client : active_clients) {
            if (client.second.username == username) {
                groups[group_name].insert(client.first);
                client.second.groups.insert(group_name);
                std::string success_message = "Joined group " + group_name + " successfully\n";
                send(client.first, success_message.c_str(), success_message.length(), 0);
                return;
            }
        }
    }

    void leave_group(const std::string& username, const std::string& group_name) {
        std::lock_guard<std::mutex> lock(groups_mutex);
        if (groups.find(group_name) == groups.end()) {
            std::string error_message = "Error: Group " + group_name + " does not exist\n";
            for (const auto& client : active_clients) {
                if (client.second.username == username) {
                    send(client.first, error_message.c_str(), error_message.length(), 0);
                    return;
                }
            }
            return;
        }

        for (auto& client : active_clients) {
            if (client.second.username == username) {
                groups[group_name].erase(client.first);
                client.second.groups.erase(group_name);
                std::string success_message = "Left group " + group_name + " successfully\n";
                send(client.first, success_message.c_str(), success_message.length(), 0);
                return;
            }
        }
    }

    void process_command(int client_socket, const std::string& command) {
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;

        if (cmd == "/pm") {
            std::string recipient, message;
            iss >> recipient;
            std::getline(iss, message);
            if (!message.empty()) message = message.substr(1);  // Remove leading space
            send_private_message(active_clients[client_socket].username, recipient, message);
        }
        else if (cmd == "/group") {
            std::string group_name, message;
            iss >> group_name;
            std::getline(iss, message);
            if (!message.empty()) message = message.substr(1);
            send_group_message(active_clients[client_socket].username, group_name, message);
        }
        else if (cmd == "/create") {
            std::string group_name;
            iss >> group_name;
            create_group(active_clients[client_socket].username, group_name);
        }
        else if (cmd == "/join") {
            std::string group_name;
            iss >> group_name;
            join_group(active_clients[client_socket].username, group_name);
        }
        else if (cmd == "/leave") {
            std::string group_name;
            iss >> group_name;
            leave_group(active_clients[client_socket].username, group_name);
        }
        else if (cmd == "/help") {
            std::string help_message = 
                "Available commands:\n"
                "/pm <username> <message> - Send private message\n"
                "/group <group_name> <message> - Send message to group\n"
                "/create <group_name> - Create a new group\n"
                "/join <group_name> - Join a group\n"
                "/leave <group_name> - Leave a group\n"
                "/help - Show this help message\n";
            send(client_socket, help_message.c_str(), help_message.length(), 0);
        }
    }

    bool authenticate(int client_socket) {
        char buffer[BUFFER_SIZE];
        std::string username, password;
        
        // Get username
        send(client_socket, "Enter username: ", 15, 0);
        memset(buffer, 0, BUFFER_SIZE);
        recv(client_socket, buffer, BUFFER_SIZE, 0);
        username = std::string(buffer);
        username = username.substr(0, username.find('\n'));

        // Get password
        send(client_socket, "Enter password: ", 15, 0);
        memset(buffer, 0, BUFFER_SIZE);
        recv(client_socket, buffer, BUFFER_SIZE, 0);
        password = std::string(buffer);
        password = password.substr(0, password.find('\n'));
        users[username].pop_back();
        if (users.find(username) != users.end() && users[username] == password) {
            std::lock_guard<std::mutex> lock(clients_mutex);
            active_clients[client_socket] = Client{client_socket, username, std::set<std::string>()};
            std::string welcome_message = "Welcome to the server, " + username + "!\n";
            send(client_socket, welcome_message.c_str(), welcome_message.length(), 0);
            broadcast_message("Server", username + " has joined the chat", client_socket);
            return true;
        }
        
        send(client_socket, "Authentication failed\n", 20, 0);
        return false;
    }

    void handle_client(int client_socket) {
        if (!authenticate(client_socket)) {
            close(client_socket);
            return;
        }

        char buffer[BUFFER_SIZE];
        while (true) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
            
            if (bytes_received <= 0) {
                std::string username = active_clients[client_socket].username;
                {
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    active_clients.erase(client_socket);
                }
                {
                    std::lock_guard<std::mutex> lock(groups_mutex);
                    for (auto& group : groups) {
                        group.second.erase(client_socket);
                    }
                }
                broadcast_message("Server", username + " has left the chat");
                close(client_socket);
                return;
            }

            std::string message(buffer);
            message = message.substr(0, message.find('\n'));

            if (message[0] == '/') {
                process_command(client_socket, message);
            } else {
                broadcast_message(active_clients[client_socket].username, message);
            }
        }
    }

public:
    ChatServer() {
        load_users("users.txt");
    }

    void start() {
        int server_socket;
        struct sockaddr_in server_addr, client_addr;
        socklen_t client_len = sizeof(client_addr);

        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            std::cerr << "Error creating socket" << std::endl;
            return;
        }

        int opt = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
            std::cerr << "Error setting socket options" << std::endl;
            return;
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(PORT);

        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Error binding socket" << std::endl;
            return;
        }

        if (listen(server_socket, 10) < 0) {
            std::cerr << "Error listening on socket" << std::endl;
            return;
        }

        std::cout << "Server started on port " << PORT << std::endl;

        while (true) {
            int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
            if (client_socket < 0) {
                std::cerr << "Error accepting connection" << std::endl;
                continue;
            }

            std::thread(&ChatServer::handle_client, this, client_socket).detach();
        }

        close(server_socket);
    }
};

int main() {
    ChatServer server;
    server.start();
    return 0;
}
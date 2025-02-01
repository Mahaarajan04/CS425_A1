#include <iostream>
#include <fstream>
#include<sstream>
#include <unordered_map>
#include<map>
#include<thread>
#include<mutex>
#include <cstring>
#include<vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <bits/stdc++.h>
//Things to keep in mind... client leaves by ctrl+C... try to use classes last mein
//Give the user list of groups and active users
//handle leave server fr grps coz socket num can change on rejoin
//handle empty grps
//Use locks on common resources

#define PORT 12345
#define BUFFER_SIZE 1024

//defining global variables that are common to all
using namespace std;
map<string,string>cred;
map<string,int>active;
map<int,string>active_inv;
map<string,vector<int>>groups;

//Function to load credentials
int load_cred(string fname){
    cred.clear();
    string username, password,line;
    fstream file (fname,ios::in);
    if(file.is_open()){
        while(getline(file, line)){
            line.pop_back();
            stringstream str(line);
            //cout<<line<<endl;
            getline(str, username, ':');
            getline(str, password, ':');
            //password.pop_back();
            //cout<<password<<endl<<username<<endl<<password.length()<<(int)password[password.length()-1]<<endl<<endl;
            cred[username]=password;
        }
        if(cred.empty()){
            std::cout<<"It is an empty file\n";
            return 0;
        }
        return 1;
    }
    else std::cout<<"Could not open the file\n";
    return 0;
}
//Function to handle a client leaving
void leave(int client_socket){
    string username=active_inv[client_socket];
    active.erase(username);
    active_inv.erase(client_socket);
    cout<<username+" has exited"<<endl;
    close(client_socket);
}

//Function to process the commands
void process_command(int client_socket, const std::string& command) {
    string cmd="", msg="";
    int flag;
    size_t spacePos = command.find(' '); // Find first space
    if (spacePos != std::string::npos) {
        cmd = command.substr(0, spacePos); 
        msg=command.substr(spacePos+1, command.length());// Extract first word
    
    } else {
        cmd = command; // If no space, take the whole string
    }
    std::string recipient, message;
    if (cmd == "/msg") {
        // u can text urself... incl this as a feature in the readme
        size_t spacePos = msg.find(' '); // Find first space
        if (spacePos != std::string::npos) {
            recipient = msg.substr(0, spacePos); 
            message = msg.substr(spacePos+1, command.length());// Extract first word
        }
        else{
            message="The message does not follow the standards\n";
            send(client_socket, message.c_str(), message.size(), 0);
            return;
        }
        if(message.empty()){
            message="The message does not follow the standards\n";
            send(client_socket, message.c_str(), message.size(), 0);
            return;
        }
        if(active.find(recipient)==active.end()){
            message="Not a valid active recipient!";
            send(client_socket, message.c_str(), message.size(), 0);
            return;
        }
        message = "pm>> "+active_inv[client_socket]+": "+message;
        if(send(active[recipient], message.c_str(), message.size(), 0)){
            message = "Your message has been sent!";
            send(client_socket, message.c_str(), message.size(), 0);
        }else{
            message = "Oops!";
            send(client_socket, message.c_str(), message.size(), 0);
        }
    }
    else if (cmd == "/broadcast") {
        message= "broadcast>> "+active_inv[client_socket]+": "+msg;
        flag=1;
        for(auto it:active){
            if(it.second!=client_socket){
                if(!(send(it.second, message.c_str(), message.size(), 0))){
                    message = "Oops!";
                    send(client_socket, message.c_str(), message.size(), 0);
                    flag=0;
                    break;
                }
            }
        }
        if(flag==1){
            message = "Your message has been sent!";
            send(client_socket, message.c_str(), message.size(), 0);
        }
    }
    else if (cmd == "/group_msg") {
        string grpname = "";
        size_t spacePos = msg.find(' '); // Find first space
        if (spacePos != std::string::npos) {
            grpname = msg.substr(0, spacePos); 
            message = msg.substr(spacePos+1, command.length());// Extract first word
        }
        else{
            message="The message does not follow the standards";
            send(client_socket, message.c_str(), message.size(), 0);
            return;
        }
        if(message.empty()){
            message="The message does not follow the standards\n";
            send(client_socket, message.c_str(), message.size(), 0);
            return;
        }
        if(groups.find(grpname)==groups.end()){
            message = "This group does not exist!!";
            send(client_socket, message.c_str(), message.size(),0);
            return;
        }
        if(find(groups[grpname].begin(),groups[grpname].end(),client_socket)==groups[grpname].end()){
            message = "You are not a part of this group";
            send(client_socket, message.c_str(), message.size(),0);
            return;
        }
        message="["+grpname+"] "+active_inv[client_socket]+": "+message;
        flag=1;
        for(auto it:groups[grpname]){
            if(it!=client_socket){
                if(!(send(it, message.c_str(), message.size(), 0))){
                    message = "Oops!";
                    send(client_socket, message.c_str(), message.size(), 0);
                    flag=0;
                    break;
                }
            }
        }
        if(flag==1){
            message = "Your group message has been sent!";
            send(client_socket, message.c_str(), message.size(), 0);
        }  
    }
    else if (cmd == "/create_group") {
        groups[msg].push_back(client_socket);
        message = msg + " group has been created.";
        send(client_socket, message.c_str(), message.size(),0);
    }
    else if (cmd == "/join_group") {
        if(groups.find(msg)!=groups.end()){
            if (std::find(groups[msg].begin(), groups[msg].end(), client_socket) == groups[msg].end()) {
                groups[msg].push_back(client_socket);
                std::string message = "You have been added to the group.";
                send(client_socket, message.c_str(), message.size(), 0);
            }
            else{
                std::string errmsg = "You have already joined the group " + msg;
                send(client_socket, errmsg.c_str(), errmsg.size(), 0);
            }
        }
        else{
            message = "This group does not exist!!";
            send(client_socket, message.c_str(), message.size(),0);
        }
    }
    else if (cmd == "/leave_group") {
        if(groups.find(msg)!=groups.end()){
            if (std::find(groups[msg].begin(), groups[msg].end(), client_socket) != groups[msg].end()) {
                groups[msg].erase(find(groups[msg].begin(),groups[msg].end(),client_socket));
                std::string message = "You have successfully left the group ["+ msg+"]";
                send(client_socket, message.c_str(), message.size(), 0);
            }
            else{
                std::string errmsg = "You are not a part of the group " + msg;
                send(client_socket, errmsg.c_str(), errmsg.size(), 0);
            }
        }
        else{
            message = "This group does not exist!!";
            send(client_socket, message.c_str(), message.size(),0);
        }
    }
    else if (cmd == "/help") {
        std::string help_message = 
            "Available commands:\n"
            "/msg <username> <message> - Send private message\n"
            "/broadcast <message> - Send message to all the active users\n"
            "/group_msg <group_name> <message> - Send message to group\n"
            "/create_group <group_name> - Create a new group\n"
            "/join_group <group_name> - Join a group\n"
            "/leave_group <group_name> - Leave a group\n"
            "/help - Show this help message";
        send(client_socket, help_message.c_str(), help_message.length(), 0);
    }
    else{
        message="The message does not follow the standards";
        send(client_socket, message.c_str(), message.size(), 0);
    }
}

//Function to handle a client connection
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    // Request username
    std::string message = "Enter username: ";
    send(client_socket, message.c_str(), message.size(), 0);
    recv(client_socket, buffer, BUFFER_SIZE, 0);
    std::string username(buffer);
    
    // Request password
    memset(buffer, 0, BUFFER_SIZE);
    message = "Enter password: ";
    send(client_socket, message.c_str(), message.size(), 0);
    recv(client_socket, buffer, BUFFER_SIZE, 0);
    std::string password(buffer);
    
    // Authentication
    int flag=0;
    if (cred.find(username) != cred.end() && cred.at(username) == password) {
        if(active.find(username)!=active.end()){
            message= "You are already logged in!!";
        }
        else{
            message = "Welcome to the server";
            active[username] = client_socket;
            active_inv[client_socket]=username;
            flag=1;
            cout<<username<<" joined the server\n";
            //send a broadcast that so and so has joined
        }
    } 
    else {
        message = "Authentication failed";
    }
    send(client_socket, message.c_str(), message.size(), 0);
    if(flag==0){
        close(client_socket);
        //cout<<username+" has exited"<<endl;
        return;
    }
    // int flag=1;
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            leave(client_socket);
            return;
        }
        std::string message(buffer);
        message = message.substr(0, message.find('\n'));
        process_command(client_socket, message);
    }

    close(client_socket);
    cout<<username+" has exited"<<endl;
    return;
}

int main(){
    int flag;
    flag=load_cred("users.txt");
    if(flag==0) return 0;
    int server_socket, client_socket;
    sockaddr_in server_address{}, client_address{};
    socklen_t client_len = sizeof(client_address);
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "Error creating socket." << std::endl;
        return 1;
    }

    //set options
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        std::cerr << "Error setting socket options" << std::endl;
        return 0;
    }
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);
    
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        std::cerr << "Binding failed." << std::endl;
        return 1;
    }
    
    //Listen
    listen(server_socket, 5);
    std::cout << "Server listening on port " << PORT << std::endl;
    
    while (true) {
        int client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_len);
        if (client_socket < 0) {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }
        std::thread(handle_client,client_socket).detach();
    }
    close(server_socket);
    return 0;
}
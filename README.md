# **CS425 Assignment 1: Chat Server with Groups and Private Messages**

## **Team** 
| **Name**                     | **Roll Number** | **Email**
|------------------------------|-----------------|---------------
| Mahaarajan J                 | 220600          |mahaarajan22@iitk.ac.in
| Wattamwar Akanksha Balaji    | 221214          |akankshab22@iitk.ac.in

## Instructions 
To run the application locally, follow the following steps &rarr;
1. Go to the directory where all the files are located and compile the client and server codes using the make file.
```bash
make
```
2. Start the server on one terminal.
```bash
./server_grp
```
3.Now open another terminal to start the client connections. Ensure that the server is started before executing client code. 
```bash
./client_grp
```
4. Now type the commands in the client terminal.
5. For every new client, you need to open another terminal if the current one is being used.

## **Features**

### Implemented Features
- Implemented a TCP-based multi-threaded server for handling multiple clients.
- User authentication via `users.txt` file containing the login credentials
- Messaging system supporting:
  - Broadcast messages (`/broadcast <message>`).
  - Private messages (`/msg <username> <message>`).
  - Group messages (`/group_msg <group name> <message>`).
- Group management:
  - Creating groups (`/create_group <group name>`).
  - Joining groups (`/join_group <group name>`).
  - Leaving groups (`/leave_group <group name>`).
- Miscellaneous
    - Listing all the possible commands with syntax to the client (`/help`) 
    - For the user to exit the chat server(`/exit`)
- Thread-safe client handling using `mutex` locks for synchronization.

### Not Implemented Features
- Advanced error handling for network failures.
- Message and group history persistence across sessions.
- Admin controls for group management.

## **Assumptions and Conventions**
- The data of all the groups a user is part of is forgotten once the user exits the server.
- User can't send a message to self.
- Empty groups are not deleted and active users can join them.
- A user once logged in can't login from a different terminal until he exits from that terminal.
- The chat server handles user exit only using Crtl+C or /exit command
- The list of users can't be modified once the server is on
- Groups of the same name (across the server) can't be created
- Messages can only be sent to active users
- Sever binds to a pre-defined port number of 12345

## **Design Decisions**

### Multi-threading Model
- **Decision:** We have created a new thread for each client connection.
- **Reason:** This allows handling multiple clients concurrently, ensuring non-blocking communication for all clients with the server. 

### Synchronization Mechanism
- **Decision:** Used `mutex` for shared resource protection.
- **Reason:** Prevents data races when multiple threads modify shared data structures like maps which are used to store clients and groups data.

### Maintaining multiple shared resources
- We maintain multiple maps for a faster lookup. These are shared among all the threads The exact maps used are:
    - cred: A map with usernames as keys and password as values
    - active: A map with usernames as keys and socket id as values
    - active_inv: A map with socket ids as keys and usernames as values
    - groups: A map with groupnames as keys and a vector of socket ids as values (representing the members of the group)
    - grp_ind: A map with socket ids as keys and a vector of strings as values (representing the groups that the user is part of)

## **Implementation**

### Key Functions
- `handle_client(int client_socket)`: Handles communication with an individual client as a seperate thread. Authenticates the client (disconnects if fails) as well.
- `process_command(int client_socket, const std::string& command)`: Handles indivudual commands that a client posts.
- `broadcast(string message, int client_socket))`: Sends a message to all connected clients except for the one sending the message.
- `private_msg(int client_socket,string message, string recipient)`: Sends a direct message to the recipient from the client.
- `grp_msg(int client_socket, string message, string grpname)`: Sends a message to all group members except for the one sending the message.
- `create_grp(int client_socket,string grp_name)`: Creates a new group with the client as its sole member.
- `join_grp(int client_socket,string grp_name)`: Adds the client to an existing group.
- `leave_group(int client_socket,string grp_name)`: Removes the client from a group.

### Code Flow
1. Server starts, loads credentials using `users.txt`,  binds to the predefined socket and listens for connections.
2. Client connects and authenticates.
3. Server assigns a thread to handle the client.
4. Clients send commands, which are parsed and executed accordingly.
5. Synchronization ensures thread-safe updates to shared data.
6. Client disconnects, and server removes its entry from active clients and groups.

## **Testing**

### Correctness Testing
- Verified authentication using valid and invalid credentials.
- Checked command execution for all message types.
- Ensured group creation, joining, and leaving functionality.

## **Challenges and Solutions**

### Multi-threading Bugs
- **Issue:** Data races when modifying shared structures.
- **Solution:** Implemented `mutex` locks to ensure thread safety.

### Server Disconnection
- **Issue:** Server disconnections did not allow for the server to run again due to inability to bind to the same port.
- **Solution:** Set the attribute setsockopt to SO_REUSEADDR so that the same port number is available for reuse.  

## **Server Restrictions**
- Maximum concurrent clients: Limited by system resources. We have set the maximum number of clients connected to 50 as a part of the listen call.
- Maximum groups: No hard limit, but memory usage constrains scalability.
- Maximum members per group: No enforced limit, but practical constraints apply.
- Maximum message size: 1024 bytes per message. Enforced by setting the buffer size.
- A user cannot message themselves.

## **Individual Contributions**

### Mahaarajan J: 220600
- Designed the start up routine and initialised the threads.
- Implemented the features such as private messaging, broadcasting.
- Cleaned up the code.

### Wattamwar Akanksha Balaji: 221214
- Implemented locks for synchronisation and basic commands processing.
- Implemented the group management features along with group messaging.
- Prepared the README file.

## **Sources**
- GFG guide to Socket Programming.
- C++ documentation for `thread` and `mutex`.
- Stack Overflow discussions on socket programming and synchronization.
- Classroom codes shared on the git repository.

## **Declaration**
We hereby declare that our code is original and does not involve plagiarism. The assignment was completed following the academic integrity guidelines.

## **Feedback**
The assignment served as a comprehensive tool to understand socket programming :)

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <fstream>
#define DEFAULTFDVALUE -1
#define NUMBOFARGS 2
#define BUFFERSIZE 1024
#define EOFINDICATOR "EOF\n"

using namespace std;

class Server{
public:
    // class members
    int socketFD;
    const string socketPath;

    // Constructor
    Server(const string socketPath) : socketFD(DEFAULTFDVALUE),socketPath(socketPath){}
    // Destructor
    ~Server(){ close_socket(socketFD,socketPath);}

    // function prototypes
    int create_socket();
    void bind_socket(int socketFD, const string& socketPath);
    void listen_on_socket(int socketFD,int maxConnections);
    int accept_connection(int socketFD);
    string handle_request(int clientFD);
    vector<string> locate_file_contents(const string& fileName);
    void send_response(int clientFD,vector<string> fileContents);
    void close_socket(int socketFD,const string &socketPath);
};

// flag to decide when program is done
volatile sig_atomic_t exit_flag = 0;

// Signal handler function
void sigint_handler(int signum) {
    exit_flag = 1;
}

// Function that sets up signal handler
// This function is purely based off of the example code from
// https://github.com/programming101dev/c-examples/blob/main/domain-socket/read-write/server.c
void setup_signal_handler() {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Install the SIGINT handler
    // if this fails, exit program right here
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char*argv[]) {

    // Setup signal handler
    setup_signal_handler();

    // validate command line arguments
    if(argc != NUMBOFARGS){
        cerr << "Incorrect amount of command line arguments, only provide socketPath" << endl;
        return EXIT_FAILURE;
    }
    string socketPath = argv[1];

    cout << "Server has started" << endl;

    Server *server = new Server(socketPath);
    int socketFD = server->create_socket();
    server->bind_socket(socketFD, socketPath);
    server->listen_on_socket(socketFD, SOMAXCONN);

    // Continuously accept clients until signal doesn't go off
    while (!exit_flag) {
        int clientFD = server->accept_connection(socketFD);

        if (clientFD == -1) {
            // ensure the signal isn't flagged
            if (exit_flag) {
                break;
            }
            // If there was an error accepting a client then try again
            continue;
        }

        string fileName = server->handle_request(clientFD);
        if (fileName == "") {
            cerr << "Error: could not retrieve filename" << endl;
            continue;
        }

        vector<string> fileContents = server->locate_file_contents(fileName);
        if (fileContents.empty()) {
            cerr << "Error obtaining file contents: " << strerror(errno) << endl;
        } else {
            cout << "File contents obtained successfully" << endl;
        }

        // client socket will shut itself down after receiving a response
        server->send_response(clientFD, fileContents);
    }

    delete(server);

    return 0;
}

int Server::create_socket() {

    // pre-caution so address isn't already in use
    unlink(socketPath.c_str());

    // initialize the socket
    socketFD = socket(AF_UNIX,SOCK_STREAM,0);
    // check for error
    if(socketFD == -1){
        cerr << "Error in creating socket: " << strerror(errno) <<endl;
        exit(EXIT_FAILURE);
    }
    cout << "Socket created succesfully" << endl;
    return socketFD;
}

void Server::bind_socket(int socketFD, const string &socketPath) {

    // initialize sockaddr_un structure
    struct sockaddr_un addr;
    memset(&addr,0,sizeof(addr));
    addr.sun_family = AF_UNIX;

    // copy socket path into struct
    strncpy(addr.sun_path,socketPath.c_str(),sizeof(addr.sun_path -1));

    // attempt binding and check for error
    if(bind(socketFD,(struct sockaddr*)&addr,sizeof(addr))==-1){
        cerr << "Error in binding socket: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }

    cout << "Socket succesfully binded at " << socketPath << endl;
}

void Server::listen_on_socket(int socketFD, int maxConnections) {
    if(listen(socketFD,maxConnections) == -1){
        cerr << "Error in listening on socket: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }
    cout << "Listening on socket for connections" << endl;
}

int Server::accept_connection(int socketFD) {

    // store client info
    struct sockaddr_un client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    int clientFD = accept(socketFD,(struct sockaddr*)&client_addr,&client_addr_size);
    if(clientFD == -1){
        cerr << "Error connecting to a new client: " << strerror(errno) << endl;
        return -1;
    }

    cout << "New client connected succesfully!" << endl;
    return clientFD;
}

string Server::handle_request(int clientFD) {

    // buffer to hold fileName
    char buffer[BUFFERSIZE];
    memset(buffer,0,sizeof(buffer));
    ssize_t requestData = read(clientFD,buffer,sizeof(buffer) - 1);

    if(requestData < 0 || requestData == 0){
        cerr << "Error reading request from client: " << strerror(errno) << endl;
        return "";
    }

    // convert buffer to string
    buffer[requestData] = '\0';
    string fileName = buffer;

    cout << "Requested filename from client is: " << fileName << endl;
    return fileName;
}

vector<string> Server::locate_file_contents(const string &fileName) {

    vector<string> fileContents;

    // construct the relative path to include directory then find file
    string filePath = "../include/" + fileName;
    ifstream file(filePath);

    if(!file.is_open()){
        cerr << "Error finding or opening "<< fileName << strerror(errno) << endl;
        return fileContents;
    }

    // extract each line and copy over
    string line;
    while(getline(file,line)){
        fileContents.push_back(line);
    }

    file.close();
    return fileContents;
}

void Server::send_response(int clientFD, vector <string> fileContents) {
    string responseMessage;

    // loop through vector and copy over contents
    for(const string& line:fileContents){
        responseMessage += line + "\n";
    }
    // add the end of file indicator and send response
    responseMessage += EOFINDICATOR;
    ssize_t responseBytes = send(clientFD,responseMessage.c_str(),responseMessage.size(),0);

    if(responseBytes == -1){
        cerr << "Error sending file contents back: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }

    cout << "File contents sent back successfully to client" << endl;
}

void Server::close_socket(int socketFD,const string &socketPath) {
    if(close(socketFD) == -1){
        cerr << "Error closing socket: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }
    // if not already unlinked, do it now
    if (access(socketPath.c_str(), F_OK) == 0) {
        if (unlink(socketPath.c_str()) == -1) {
            cerr << "Error unlinking socket: " << strerror(errno) << endl;
        }
    } else {
        cout << "Socket file does not exist, no need to unlink." << endl;
    }
    cout << "Socket closed succesfully" << endl;
}

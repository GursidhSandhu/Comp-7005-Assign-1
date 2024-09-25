#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>
#include <sstream>
#define DEFAULTFDVALUE -1
#define NUMBOFARGS 3
#define BUFFERSIZE 1024
#define EOFINDICATOR "EOF\n"

using namespace std;

class Client{
public:
    // class members
    int socketFD;
    const string socketPath, fileName;

    // Constructor
    Client(const string socketPath, const string fileName):
    socketFD(DEFAULTFDVALUE),socketPath(socketPath),fileName(fileName){}
    // Destructor
    ~Client(){ close_socket(socketFD);}

    // function prototypes
    int create_socket();
    void attempt_socket_connection(int socketFD,const string& socketPath);
    void send_request(int socketFD, const string& fileName);
    vector<string> handle_response(int socketFD);
    void print_file_contents(vector<string> fileContents);
    void close_socket(int socketFD);
};
int main(int argc, char*argv[]) {
    // validate command line arguments
    if(argc != NUMBOFARGS){
        cerr << "Incorrect amount of command line arguments,only provide socketPath and fileName" << endl;
        return EXIT_FAILURE;
    }
    string socketPath = argv[1];
    string fileName = argv[2];

    cout << "Client has started" << endl;

    Client *client = new Client(socketPath,fileName);
    int socketFD = client->create_socket();
    client->attempt_socket_connection(socketFD,socketPath);
    client->send_request(socketFD,fileName);

    vector<string> fileContents = client->handle_response(socketFD);
    if(fileContents.empty()){
        cerr << "Error obtaining file contents from response. Ensure correct file name and extension" << endl;
        exit(EXIT_FAILURE);
    }else{
        cout << "File contents obtained succesfully" << endl;
    }
    client->print_file_contents(fileContents);
    delete(client);

    // Return 0 to indicate the program ended successfully
    return 0;
}

int Client::create_socket() {

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

void Client::attempt_socket_connection(int socketFD, const string &socketPath) {

    // initialize sockaddr_un structure
    struct sockaddr_un addr;
    memset(&addr,0,sizeof(addr));
    addr.sun_family = AF_UNIX;

    // copy socket path into struct
    strncpy(addr.sun_path,socketPath.c_str(),sizeof(addr.sun_path -1));

    // attempt connecting
    if(connect(socketFD,(struct sockaddr*)&addr,sizeof(addr)) == -1){
        cerr << "Error in connecting to server socket: "<< strerror(errno) <<endl;
        exit(EXIT_FAILURE);
    }

    cout << "Succesfully connected at " << socketPath << endl;
}

void Client::send_request(int socketFD, const std::string &fileName) {

    // construct request and send
    string requestContents = fileName;
    ssize_t request = send(socketFD,requestContents.c_str(),requestContents.size(),0);

    if(request == -1){
        cerr << "Error in sending request to server socket: "<< strerror(errno) <<endl;
        exit(EXIT_FAILURE);
    }

    cout << "Succesfully sent request containing: " << fileName << " to the server" << endl;
}

vector<string> Client::handle_response(int socketFD) {

    vector<string> fileContents;
    string response,line;
    char buffer[BUFFERSIZE];
    ssize_t responseData;

    // iterate through all the data in response
    while(true){
        responseData = read(socketFD,buffer,BUFFERSIZE-1);
        if(responseData < 0 || responseData == 0){
            cerr << "Error reading response from server: " << strerror(errno) << endl;
            fileContents.clear();
            break;
        }
        buffer[responseData] = '\0';
        response += buffer;

        // if EOF is found, then remove it outside loop
        if(response.find(EOFINDICATOR) != string::npos){
            break;
        }
    }


    istringstream responseStream(response);
    while(getline(responseStream,line)){
        if(line != "EOF"){
            fileContents.push_back(line);
        }else{
            break;
        }
    }

    return fileContents;
}

void Client::print_file_contents(vector <std::string> fileContents) {
    cout << "The contents of " << fileName << " obtained from the server are:\n" << endl;
    for(const string& line:fileContents){
        cout << line << endl;
    }
    cout << "\n" << endl;
}

void Client::close_socket(int socketFD) {
    if(close(socketFD) == -1){
        cerr << "Error closing socket: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }
    cout << "Socket closed succesfully" << endl;
}
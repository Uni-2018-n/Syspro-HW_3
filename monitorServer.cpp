#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

int main(int argc, const char** argv) {
    int portNum, socketBufferSize, cyclicBufferSize, sizeOfBloom, numThreads;

    if(argc <= 10){//simple steps to set the command line arguments to parameters
        cout << "Error! Wrong parameters:" << endl;
        cout << "monitorServer -p port -t numThreads -b socketBufferSize -c cyclicBufferSize -s sizeOfBloom path1 path2 ... pathn" << endl;
        return -1;
    }
    int i;
    for(i=0;i<argc;i++){
        if(strcmp(argv[i], "-p")==0){
            portNum = atoi(argv[i+1]);
        }else if(strcmp(argv[i], "-t")==0){
            numThreads = atoi(argv[i+1]);
        }else if(strcmp(argv[i], "-b")==0){
            socketBufferSize = atoi(argv[i+1]);
        }else if(strcmp(argv[i], "-c")==0){
            cyclicBufferSize = atoi(argv[i+1]);
        }else if(strcmp(argv[i], "-s")==0){
            sizeOfBloom = atoi(argv[i+1]);
            break;
        }
    }

    int dirLen = argc - i - 1; //argc - (index of -s) - (first argument is name of executable)
    char* pathToDirs[dirLen];
    int j=0;
    for(i = i+2;i<argc;i++){
        pathToDirs[j] = new char[strlen(argv[i])+1]();
        strcpy(pathToDirs[j], argv[i]);
        j++;
    }

    struct hostent *rem;
    struct in_addr **addr_list;
    if((rem = gethostbyname("localhost")) == NULL){
        cout << "ERROR" << endl;
    }

    int socke;
    struct sockaddr_in serveradr;
    struct sockaddr* serveradrptr = (struct sockaddr*)&serveradr;
    if((socke = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        cout << "error" << endl;
        exit(1);
    }

    serveradr.sin_family = AF_INET;
    memcpy(&serveradr.sin_addr, rem->h_addr, rem->h_length);
    serveradr.sin_port = htons(portNum);

    if(connect(socke, serveradrptr, sizeof(serveradr)) < 0){
        cout << "error" << endl;
        exit(1);
    }

    write(socke, ("test"+to_string(getpid())).c_str(), 1024);

    close(socke);

    cout << "DONE!" << endl;
    return 0;
}
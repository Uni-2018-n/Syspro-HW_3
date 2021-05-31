#include <asm-generic/socket.h>
#include <iostream>
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <stdlib.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>

#include "fromProjectOneAndTwo/fromProjectTwo/sList.hpp"

using namespace std;

#define PORT 15000

int main(int argc, const char** argv) {
    int numMonitors, socketBufferSize, cyclicBufferSize, sizeOfBloom, numThreads;
    char* pathToDirs;

    if(argc != 13){//simple steps to set the command line arguments to parameters
        cout << "Error! Wrong parameters:" << endl;
        cout << "./travelMonitorClient –m numMonitors -b socketBufferSize -c cyclicBufferSize -s sizeOfBloom -i input_dir -t numThreads" << endl;
        return -1;
    }
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i], "-m")==0){
            numMonitors = atoi(argv[i+1]);
        }else if(strcmp(argv[i], "-b")==0){
            socketBufferSize = atoi(argv[i+1]);
        }else if(strcmp(argv[i], "-c")==0){
            cyclicBufferSize = atoi(argv[i+1]);
        }else if(strcmp(argv[i], "-s")==0){
            sizeOfBloom = atoi(argv[i+1]);
        }else if(strcmp(argv[i], "-i")==0){
            pathToDirs = new char[strlen(argv[i+1])+1]();
            strcpy(pathToDirs, argv[i+1]);
        }else if(strcmp(argv[i], "-t")==0){
            numThreads = atoi(argv[i+1]);
            break;
        }
    }

    DIR *inputDir;
    if((inputDir = opendir(pathToDirs))== NULL){//open the input_dir provided in the command line to read the countries
        perror("travelMonitorClient: Cant open dir\n");
        return -1;
    }

    struct dirent *dirent;
    SLHeader countryList;//create a list with all the countries
    while((dirent=readdir(inputDir)) != NULL){//for each item available in the input dir
        if(strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..")==0){//skip the two subdirs indicating the previus dir
            continue;
        }
        countryList.insert(dirent->d_name);//insert the countrie's name to the list
    }

    int activeMonitors;
    if(countryList.count < numMonitors){//in case the count of the countries is less than the monitors
        activeMonitors = countryList.count;//we only need as many monitors as countries
    }else{
        activeMonitors = numMonitors;//else we use as many monitors as user provided from command line
    }

    char** toGiveArgs[activeMonitors];
    int i=0;
    int temp;

    if(int(countryList.count/activeMonitors) == float(float(countryList.count)/float(activeMonitors))){
        temp = 0;
    }else{
        temp = 1;
    }
    for(i=0;i<activeMonitors;i++){
        int currSize;
        if(i==activeMonitors-1){
            currSize = 10+int(countryList.count/activeMonitors);
        }else{
            currSize = 10+int(countryList.count/activeMonitors)+temp;
        }

        toGiveArgs[i]= new char*[currSize+1];
        string curr = "-p";
        toGiveArgs[i][0] = new char[curr.length()+1]();
        strcpy(toGiveArgs[i][0], curr.c_str());

        curr = to_string(PORT+i);
        toGiveArgs[i][1] = new char[curr.length()+1]();
        strcpy(toGiveArgs[i][1], curr.c_str());

        curr = "-t";
        toGiveArgs[i][2] = new char[curr.length()+1]();
        strcpy(toGiveArgs[i][2], curr.c_str());

        curr = to_string(numThreads);
        toGiveArgs[i][3] = new char[curr.length()+1]();
        strcpy(toGiveArgs[i][3], curr.c_str());

        curr = "-b";
        toGiveArgs[i][4] = new char[curr.length()+1]();
        strcpy(toGiveArgs[i][4], curr.c_str());

        curr = to_string(socketBufferSize);
        toGiveArgs[i][5] = new char[curr.length()+1]();
        strcpy(toGiveArgs[i][5], curr.c_str());

        curr = "-c";
        toGiveArgs[i][6] = new char[curr.length()+1]();
        strcpy(toGiveArgs[i][6], curr.c_str());

        curr = to_string(cyclicBufferSize);
        toGiveArgs[i][7] = new char[curr.length()+1]();
        strcpy(toGiveArgs[i][7], curr.c_str());

        curr = "-s";
        toGiveArgs[i][8] = new char[curr.length()+1]();
        strcpy(toGiveArgs[i][8], curr.c_str());

        curr = to_string(sizeOfBloom);
        toGiveArgs[i][9] = new char[curr.length()+1]();
        strcpy(toGiveArgs[i][9], curr.c_str());

        int j=10;
        for(j=10;j<currSize;j++){
            curr = countryList.popFirst();
            toGiveArgs[i][j] = new char[curr.length()+1]();//store the country to the array
            strcpy(toGiveArgs[i][j], curr.c_str());
        }
        toGiveArgs[i][j] = nullptr;
    }

    int sockets[activeMonitors];
    int socketfds[activeMonitors];

    struct sockaddr_in address;
    struct sockaddr* addressptr = (struct sockaddr *)&address;
    int opt =1;
    int addrlen = sizeof(address);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    
    for(i=0;i<activeMonitors;i++){
        if((socketfds[i] = socket(AF_INET, SOCK_STREAM, 0)) <0){
            perror("error");
            exit(1);
        }
        address.sin_port = htons(atoi(toGiveArgs[i][1]));

        if(setsockopt(socketfds[i], SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
            perror("error");
            exit(1);
        }

        if(bind(socketfds[i], addressptr, sizeof(address)) < 0){
            perror("error");
            exit(1);
        }

        if(listen(socketfds[i], 1) < 0){
            perror("error");
            exit(1);
        }

        pid_t pid;
        switch(pid=fork()){//fork the new child
        case -1:
            perror("Parent: Fork ERROR\n");
            exit(-1);
            break;
        case 0:
            execvp("./monitorServer", toGiveArgs[i]);//and execlp the new child to the ./monitor process
            perror("Parent: Execlp ERROR\n");
            break;
        default:
            break;
            // monitorPids[i]=pid;//in the parent process store the pid for use later
        }

        if((sockets[i] = accept(socketfds[i], NULL, NULL)) < 0){
            perror("error");
            exit(1);
        }

    }

    for(i=0;i<activeMonitors;i++){
        char buff[1024];
        read(sockets[i], buff, 1024);
        cout << "parent: " << buff << endl;

    }


    for(i=0;i<activeMonitors;i++){
        int status;
        wait(&status);
        cout << status << endl;
        close(socketfds[i]);
    }

    cout << "PARENT DONE!" << endl; 
    return 0;
}
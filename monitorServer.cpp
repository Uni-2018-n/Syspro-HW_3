#include <dirent.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "fromProjectOneAndTwo/fromProjectOne/generalList.hpp"
#include "funcs.hpp"
#include "commands.hpp"

using namespace std;

string* pool;
int sizePool;
int poolStart;
int poolEnd;
int currSizePool;
pthread_mutex_t poolMtx;
pthread_cond_t poolFull;
pthread_cond_t poolEmpty;
pthread_cond_t poolDone;
pthread_mutex_t currSizeMtx;

int threadStop;
GlistHeader* main_list;
pthread_mutex_t mainListMtx;



void* threadFunction(void* ptr);

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

    int dirLen = argc - i - 2; //argc - (index of -s) - (first argument is name of executable)
    string pathToDirs[dirLen];
    int j=0;
    for(i = i+2;i<argc;i++){
        pathToDirs[j] = argv[i];
        j++;
    }

    pool = new string[cyclicBufferSize];
    for(i=0;i<cyclicBufferSize;i++){
        pool[i] = "-1";
    }
    sizePool = cyclicBufferSize;
    currSizePool =0;
    poolStart =0;
    poolEnd = -1;
    pthread_mutex_init(&poolMtx, 0);
    pthread_mutex_init(&currSizeMtx, 0);
    pthread_cond_init(&poolEmpty, 0);
    pthread_cond_init(&poolDone, 0);
    pthread_cond_init(&poolFull, 0);


    threadStop = 1;

    pthread_t threads[numThreads];
    for(i=0;i<numThreads;i++){
        pthread_create(&threads[i], 0, threadFunction, 0);
    }

    main_list = new GlistHeader(sizeOfBloom);
    pthread_mutex_init(&mainListMtx, 0);

    struct hostent *rem;
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

    int countFilesOfDirs[dirLen];//array usefull for SIGUSR1 signal(see next)
    for(i=0;i<dirLen;i++){
        DIR *curr_dir;
        if((curr_dir = opendir((pathToDirs[i] + '/').c_str()))== NULL){//open the directory 
            perror("Child: Cant open dir\n");
        }
        int count=0;//simple counter to be stored later in countFilesOfDirs to count how many files there is in the country
        struct dirent *dirent;
        while((dirent=readdir(curr_dir)) != NULL){//and for each file inside the country's directory
            if(strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..")==0){
                continue;
            }
            pthread_mutex_lock(&poolMtx);
            while(currSizePool >= sizePool){
                pthread_cond_wait(&poolFull, &poolMtx);
            }
            poolEnd = (poolEnd + 1) % sizePool;
            pool[poolEnd] = pathToDirs[i]+'/'+dirent->d_name;
            currSizePool++;


            pthread_mutex_unlock(&poolMtx);
            pthread_cond_signal(&poolEmpty);
            usleep(0);
            
            count++;
        }
        countFilesOfDirs[i]=count;
        closedir(curr_dir);
    }


    pthread_mutex_lock(&mainListMtx);
    string* temp_blooms = main_list->getBlooms();//after everything is done encode all the bloom filters in a string array to be easier to send to the parent
    writeSocketInt(socke, socketBufferSize, main_list->getCountViruses());//and start sending
    for(i=0;i<main_list->getCountViruses();i++){
        writeSocketInt(socke, socketBufferSize, temp_blooms[i].length());
        int t = readSocketInt(socke, socketBufferSize);
        while(t != 0){
            writeSocketInt(socke, socketBufferSize, temp_blooms[i].length());
            t = readSocketInt(socke, socketBufferSize);
        }
        writeSocket(socke, socketBufferSize, temp_blooms[i]);
    }
    pthread_mutex_unlock(&mainListMtx);
    writeSocketInt(socke, socketBufferSize, 0);//finally inform the parent that everything is ok and ready to receive commands and signals


    int totalRequests=0;
    int acceptedRequests=0;
    int rejectedRequests=0;
    while(true){
        int currFunc= readSocketInt(socke, socketBufferSize);
            if(currFunc != -1){
                int t = handlFunctionMonitor(socke, socketBufferSize, currFunc, main_list);//by passing the command to the handler function 
                if(t == 1){//this function could return 1 or 0 if the function was a /travelRequest, if yes update the variables
                    acceptedRequests++;
                    totalRequests++;
                }else if(t == 0){
                    rejectedRequests++;
                    totalRequests++;
                }else if(t == -2){
                    break;
                }
        }
    }

    generateLogFile(dirLen, pathToDirs, totalRequests, acceptedRequests, rejectedRequests);//simply generate the log file and continue
    cout << getpid() << ": exiting..." << endl;
    delete main_list;
    delete[] temp_blooms;

    for(i=0;i<numThreads;i++){
        pthread_mutex_lock(&poolMtx);
        while(currSizePool >= sizePool){
            pthread_cond_wait(&poolFull, &poolMtx);
        }
        poolEnd = (poolEnd + 1) % sizePool;
        pool[poolEnd] = "EXIT";
        currSizePool++;
        pthread_mutex_unlock(&poolMtx);
        pthread_cond_signal(&poolEmpty);
        usleep(0);
    }

    for(i=0;i<numThreads;i++){
        threadStop = 0;
        pthread_join(threads[i], 0);
        cout << "IM HERE" << endl;
    }
    pthread_cond_destroy(&poolEmpty);
    pthread_cond_destroy(&poolFull);
    pthread_mutex_destroy(&poolMtx);
    close(socke);

    cout << "DONE!" << endl;
    return 0;
}

void* threadFunction(void* ptr){
    while((threadStop || currSizePool > 0)){

        string curr;
        pthread_mutex_lock(&poolMtx);
        while(currSizePool <= 0){
            pthread_cond_wait(&poolEmpty, &poolMtx);
        }
        curr = pool[poolStart];
        poolStart = (poolStart + 1)  % sizePool;
        currSizePool--;
        pthread_mutex_unlock(&poolMtx);

        pthread_cond_signal(&poolFull);
        usleep(0);

        if(curr == "EXIT"){
            break;
        }
        cout << pthread_self() << " " << curr << endl;

        pthread_mutex_lock(&mainListMtx);
        
        ifstream records(curr);
        if(records.fail()){
            perror("Thread: file open ERROR\n");
        }
        string line;

        while(getline(records, line)){//and provide each line inside the main list like we did in project one
            main_list->insertRecord(line, false);
        }
        records.close();

        pthread_mutex_unlock(&mainListMtx);
    }
    cout << "THREAD DONE" << endl;
    pthread_exit(0);
}
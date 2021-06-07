#include <dirent.h>
#include <fstream>
#include <cstring>
#include <netdb.h>
#include <unistd.h>

#include "funcs.hpp"
#include "commands.hpp"

using namespace std;

string* pool;//global variables for cyclic buffer
int cyclicBufferSize;
int poolStart;
int poolEnd;
int currSizePool;
pthread_mutex_t poolMtx;
pthread_cond_t poolFull;
pthread_cond_t poolEmpty;

GlistHeader* main_list;//since we need threads to access the main_list we need to make it a global variable
pthread_mutex_t mainListMtx;//and use a mutex for accessing it

string getCountry(string t);
void* threadFunction(void* ptr);

int main(int argc, const char** argv) {
    int portNum, socketBufferSize, sizeOfBloom, numThreads;

    if(argc <= 10){//simple steps to set the command line arguments to variables
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

    int dirLen = argc - i - 2; //argc - (index of sizeOfBloom) - 2 to find how many paths we have
    string pathToDirs[dirLen];
    int j=0;
    for(i = i+2;i<argc;i++){//fill pathToDirs array
        pathToDirs[j] = argv[i];
        j++;
    }

    pool = new string[cyclicBufferSize];//initialize pool buffer
    for(i=0;i<cyclicBufferSize;i++){
        pool[i] = "-1";
    }

    currSizePool = 0;//initialize global virables, mutexes and condition varriables
    poolStart = 0;
    poolEnd = -1;
    pthread_mutex_init(&poolMtx, 0);
    pthread_cond_init(&poolEmpty, 0);
    pthread_cond_init(&poolFull, 0);

    main_list = new GlistHeader(sizeOfBloom);
    pthread_mutex_init(&mainListMtx, 0);

    struct hostent *rem;//use localhost since the other end of the socket is in the same machine as this
    if((rem = gethostbyname("localhost")) == NULL){
        cout << "monitorServer: gethostbyname ERROR" << endl;
        exit(-1);
    }

    int socke;
    struct sockaddr_in serveradr;
    struct sockaddr* serveradrptr = (struct sockaddr*)&serveradr;
    if((socke = socket(AF_INET, SOCK_STREAM, 0)) < 0){//init socket communication
        cout << "monitorServer: socket ERROR" << endl;
        exit(-1);
    }

    serveradr.sin_family = AF_INET;
    memcpy(&serveradr.sin_addr, rem->h_addr, rem->h_length);
    serveradr.sin_port = htons(portNum);//setup correct port to use

    if(connect(socke, serveradrptr, sizeof(serveradr)) < 0){//finally send a request for the socket
        cout << "monitorServer: socket connect ERROR" << endl;
        exit(-1);
    }

    pthread_t threads[numThreads];//since we want to start reading files and adding to the cyclic buffer
    for(i=0;i<numThreads;i++){//start numThreads threads
        pthread_create(&threads[i], 0, threadFunction, 0);
    }

    int countFilesOfDirs[dirLen];//array usefull for addVaccinationRecords command
    for(i=0;i<dirLen;i++){//for each country
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
            pthread_mutex_lock(&poolMtx);//lock the cyclic buffer mutex(since we wont be the only ones need to access this data)
            while(currSizePool >= cyclicBufferSize){//incase pool is full
                pthread_cond_wait(&poolFull, &poolMtx);
            }
            poolEnd = (poolEnd + 1) % cyclicBufferSize;//when there is empty space inside the pool append new data
            pool[poolEnd] = pathToDirs[i]+'/'+dirent->d_name;
            currSizePool++;


            pthread_mutex_unlock(&poolMtx);//unlock the mutex
            pthread_cond_signal(&poolEmpty);//and signal the condition variable that there is atleast one data inside the pool
            usleep(0);
            
            count++;
        }
        countFilesOfDirs[i]=count;
        closedir(curr_dir);
    }
    for(i=0;i<numThreads;i++){//as before to interact with the cyclic pool we do the same steps, this adds a EXIT protocol message to the buffer
        pthread_mutex_lock(&poolMtx);//so the threads know that no more data is about to get inserted and exit
        while(currSizePool >= cyclicBufferSize){
            pthread_cond_wait(&poolFull, &poolMtx);
        }
        poolEnd = (poolEnd + 1) % cyclicBufferSize;
        pool[poolEnd] = "EXIT";
        currSizePool++;
        pthread_mutex_unlock(&poolMtx);
        pthread_cond_signal(&poolEmpty);
        usleep(0);
    }
    for(i=0;i<numThreads;i++){//and wait for them to close to be able to continue with no sync issues
        pthread_join(threads[i], 0);
    }


    pthread_mutex_lock(&mainListMtx);//a bit unessasary but already setup
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
        int currFunc= readSocketInt(socke, socketBufferSize);//wait for parent to send a protocol message
        if(currFunc == 106){//incase we have a 106 command, addVaccinationRecords
            currSizePool =0;//reset global variables
            poolStart =0;
            poolEnd = -1;
            for(i=0;i<numThreads;i++){//open new threads
                pthread_create(&threads[i], 0, threadFunction, 0);
            }
            for(i=0;i<dirLen;i++){
                DIR *curr_dir;
                if((curr_dir = opendir((pathToDirs[i]+'/').c_str()))== NULL){//for each country
                    perror("Child: appendData Cant open dir\n");
                }
                int count=0;
                struct dirent *dirent;
                while((dirent=readdir(curr_dir)) != NULL){//we count the files of the directory
                    if(strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..")==0){
                        continue;
                    }
                    count++;
                }
                while(countFilesOfDirs[i] < count){//and if the fileCount[i] (made previously) is diffrent from the count we just calculated
                                            //it means that there is more files inside this directory so read them and insert them to the main_list
                    countFilesOfDirs[i]++;
                    pthread_mutex_lock(&poolMtx);
                    while(currSizePool >= cyclicBufferSize){
                        pthread_cond_wait(&poolFull, &poolMtx);
                    }
                    poolEnd = (poolEnd + 1) % cyclicBufferSize;
                    pool[poolEnd] = pathToDirs[i]+'/'+getCountry(pathToDirs[i])+'-'+to_string(countFilesOfDirs[i]).c_str()+".txt";
                    currSizePool++;

                    pthread_mutex_unlock(&poolMtx);
                    pthread_cond_signal(&poolEmpty);
                    usleep(0);
                }
                closedir(curr_dir);
            }
            for(i=0;i<numThreads;i++){
                pthread_mutex_lock(&poolMtx);
                while(currSizePool >= cyclicBufferSize){
                    pthread_cond_wait(&poolFull, &poolMtx);
                }
                poolEnd = (poolEnd + 1) % cyclicBufferSize;
                pool[poolEnd] = "EXIT";
                currSizePool++;
                pthread_mutex_unlock(&poolMtx);
                pthread_cond_signal(&poolEmpty);
                usleep(0);
            }
            for(i=0;i<numThreads;i++){
                pthread_join(threads[i], 0);
            }


            pthread_mutex_lock(&mainListMtx);
            temp_blooms = main_list->getBlooms();//re-encode the blooms
            writeSocketInt(socke, socketBufferSize, main_list->getCountViruses());//and send the updated blooms to the parent process
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
        }else if(currFunc != -1){
            int t = handlFunctionMonitor(socke, socketBufferSize, currFunc, main_list);//by passing the command to the handler function 
            if(t == 1){//this function could return 1 or 0 if the function was a /travelRequest, if yes update the variables
                acceptedRequests++;
                totalRequests++;
            }else if(t == 0){
                rejectedRequests++;
                totalRequests++;
            }else if(t == -2){//case exit
                break;
            }
        }
    }

    generateLogFile(dirLen, pathToDirs, totalRequests, acceptedRequests, rejectedRequests);//simply generate the log file and continue
    delete main_list;
    delete[] temp_blooms;


    pthread_cond_destroy(&poolEmpty);
    pthread_cond_destroy(&poolFull);
    pthread_mutex_destroy(&poolMtx);
    pthread_mutex_destroy(&mainListMtx);
    close(socke);
    return 0;
}

void* threadFunction(void* ptr){
    while(true){//infinite while loop since we are stoping when we read specific message from the buffer
        string curr;
        pthread_mutex_lock(&poolMtx);//lock the mutex
        while(currSizePool <= 0){
            pthread_cond_wait(&poolEmpty, &poolMtx);//incase buffer is empty
        }
        curr = pool[poolStart];//whene there is atleast one item
        poolStart = (poolStart + 1)  % cyclicBufferSize;//read data and update variables
        currSizePool--;
        pthread_mutex_unlock(&poolMtx);//unlock mutex

        pthread_cond_signal(&poolFull);//and signal condition variable to signal that the buffer has atleast one empty space
        usleep(0);

        if(curr == "EXIT"){//check if we received a EXIT protocol message
            break;
        }

        pthread_mutex_lock(&mainListMtx);//lock main_list mutex
        ifstream records(curr);//open readed file and insert its lines into the structure
        if(records.fail()){
            perror("Thread: file open ERROR\n");
        }
        string line;

        while(getline(records, line)){//and provide each line inside the main list like we did in project one
            main_list->insertRecord(line, false);
        }
        records.close();
        pthread_mutex_unlock(&mainListMtx);//unlock the mutex 
    }
    pthread_exit(0);
}

string getCountry(string t){//function for addVaccinationRecords, receives a complete country path (relative path) and returns only the country
    return t.substr(t.find_last_of('/')+1);
}
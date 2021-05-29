#include <iostream>
#include <cstring>
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

    return 0;
}
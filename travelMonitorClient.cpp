#include <dirent.h>
#include <cstring>
#include <unistd.h>

#include <sys/wait.h>
#include <netdb.h>

#include "funcs.hpp"
#include "fromProjectOneAndTwo/fromProjectTwo/sList.hpp"
#include "parentCommands.hpp"

using namespace std;

#define PORT 15000

int main(int argc, const char** argv) {
    int numMonitors, socketBufferSize, cyclicsocketBufferSize, sizeOfBloom, numThreads;
    string pathToDirs;

    if(argc != 13){//simple steps to set the command line arguments to variables
        cout << "Error! Wrong parameters:" << endl;
        cout << "./travelMonitorClient –m numMonitors -b socketsocketBufferSize -c cyclicsocketBufferSize -s sizeOfBloom -i input_dir -t numThreads" << endl;
        return -1;
    }
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i], "-m")==0){
            numMonitors = atoi(argv[i+1]);
        }else if(strcmp(argv[i], "-b")==0){
            socketBufferSize = atoi(argv[i+1]);
        }else if(strcmp(argv[i], "-c")==0){
            cyclicsocketBufferSize = atoi(argv[i+1]);
        }else if(strcmp(argv[i], "-s")==0){
            sizeOfBloom = atoi(argv[i+1]);
        }else if(strcmp(argv[i], "-i")==0){
            pathToDirs = argv[i+1];
        }else if(strcmp(argv[i], "-t")==0){
            numThreads = atoi(argv[i+1]);
            break;
        }
    }

    DIR *inputDir;
    if((inputDir = opendir(pathToDirs.c_str()))== NULL){//open the input_dir provided in the command line to read the countries
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
    closedir(inputDir);

    int activeMonitors;
    if(countryList.count < numMonitors){//in case the count of the countries is less than the monitors
        activeMonitors = countryList.count;//we only need as many monitors as countries
    }else{
        activeMonitors = numMonitors;//else we use as many monitors as user provided from command line
    }

    int monitorPids[activeMonitors];

    int i=0;
    int temp;
    string** toGiveDirs = new string*[activeMonitors];//create an 2D array, [i][j], i indicates the index of the monitor and j the index of the country that the monitor will work on
    for(i=0;i<activeMonitors;i++){
        toGiveDirs[i] = new string[int(countryList.count/activeMonitors)+1];
    }
    char** toGiveArgs[activeMonitors];//2d array to give arguments for execvp

    if(int(countryList.count/activeMonitors) == float(float(countryList.count)/float(activeMonitors))){//to see if the last monitor has an extra country
        temp = 0;
    }else{
        temp = 1;
    }
    for(i=0;i<activeMonitors;i++){//first 10 possitions have the arguments that we want to pass to the monitors
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

        curr = to_string(cyclicsocketBufferSize);
        toGiveArgs[i][7] = new char[curr.length()+1]();
        strcpy(toGiveArgs[i][7], curr.c_str());

        curr = "-s";
        toGiveArgs[i][8] = new char[curr.length()+1]();
        strcpy(toGiveArgs[i][8], curr.c_str());

        curr = to_string(sizeOfBloom);
        toGiveArgs[i][9] = new char[curr.length()+1]();
        strcpy(toGiveArgs[i][9], curr.c_str());

        int j;
        int k=0;
        for(j=10;j<currSize;j++){//the rest of the arguments are the paths 
            string t =countryList.popFirst();
            curr = pathToDirs+'/'+t;
            toGiveArgs[i][j] = new char[curr.length()+1]();//store the country to the array
            strcpy(toGiveArgs[i][j], curr.c_str());
            toGiveDirs[i][k] = t;
            k++;
        }
        toGiveArgs[i][j] = nullptr;
    }

    int sockets[activeMonitors];//for socket init
    int socketFds[activeMonitors];//for keeping the fd for the sockets

    struct sockaddr_in address;//for socket init
    struct sockaddr* addressptr = (struct sockaddr *)&address;
    int opt =1;//used in setsockopt to setup with SO_REUSEPORT flag

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    
    for(i=0;i<activeMonitors;i++){
        if((sockets[i] = socket(AF_INET, SOCK_STREAM, 0)) <0){//init the socket
            perror("error");
            exit(1);
        }
        address.sin_port = htons(atoi(toGiveArgs[i][1]));//set the port as we set it earlier for the specific monitor

        if(setsockopt(sockets[i], SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){//setup setsockopt to use the nessasary flags
            perror("error");
            exit(1);
        }

        if(bind(sockets[i], addressptr, sizeof(address)) < 0){
            perror("error");
            exit(1);
        }

        if(listen(sockets[i], 1) < 0){//since we have 1 socket for each monitor listen only needs 1 connection request
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
            execvp("./monitorServer", toGiveArgs[i]);//execvp to be able to run executable with an array as parameters(since we dont know how many arguments we need to pass)
            perror("Parent: Execvp ERROR\n");
            break;
        default:
            if((socketFds[i] = accept(sockets[i], NULL, NULL)) < 0){//wait for child to send a socket request and accept it
                perror("error");
                exit(1);
            }
            monitorPids[i]=pid;//in the parent process store the pid for use later
            break;
        }
    }

    VirlistHeader viruses(sizeOfBloom);//after all of this is done wait for each monitor to receive the bloom filters
    for(i=0;i<activeMonitors;i++){
        int tempSize= readSocketInt(socketFds[i], socketBufferSize);
        string tempBlooms[tempSize];//1D array to store the encoded bloom filters for each virus from the monitor
        for(int j=0;j<tempSize;j++){
            int ts = readSocketInt(socketFds[i], socketBufferSize);//read the blooms with error checking in case something happen while reading
            while(ts == -1){
                writeSocketInt(socketFds[i], socketBufferSize, -1);
                ts = readSocketInt(socketFds[i], socketBufferSize);
            }
            writeSocketInt(socketFds[i], socketBufferSize, 0);
            tempBlooms[j] = readSocket(socketFds[i], ts, socketBufferSize);//and store it into a temporary string array
        }
        if(readSocketInt(socketFds[i], socketBufferSize) != 0){//read the confirmation message from the monitor
            exit(-1);
        }
        for(int j=0;j<tempSize;j++){//for each virus
            int k=tempBlooms[j].find("!");//find the ! indicator to substring the name of the virus
            VirlistNode* curr;
            if((curr = viruses.searchVirus(tempBlooms[j].substr(0, k))) == NULL){//check if the virus exists, 
                curr = viruses.insertVirus(tempBlooms[j].substr(0,k));//if not create it
            }
            tempBlooms[j].erase(0,k+1);//remove the name(and the !) of the encoded string
            curr->insertBloom(tempBlooms[j]);//and finally append the data to the bloom filter
        }
    }

    TSHeader stats;
    cout <<
    "/travelRequest citizenID date countryFrom countryTo virusName" << endl <<
    "/travelStats virusName date1 date2 [country]" << endl <<
    "/addVaccinationRecords country" << endl <<
    "/searchVaccinationStatus citizenID" << endl <<
    "/exit" << endl << endl;

    while(true){//simple switch-case but for strings
        string command;
        cout << "Waiting for command: " << flush;
        cin >> command;
        if(command == "/exit"){//exit command
            int status;
            pid_t pid;
            for(i=0;i<numMonitors;i++){
                writeSocketInt(socketFds[i], socketBufferSize, 105);//send a protocol message instead of sending a signal
                pid = waitpid(monitorPids[i], &status, 0);
                cout << "child " << (long)pid << " got exited " << status << endl;
                close(sockets[i]);
                close(socketFds[i]);
            }
            generateLogFileParent(activeMonitors, int(countryList.count/activeMonitors)+1, toGiveDirs, stats.total, stats.accepted, stats.rejected);
            for(i=0;i<activeMonitors;i++){
                delete[] toGiveDirs[i];
            }
            delete[] toGiveDirs;
            for(i=0;i<activeMonitors;i++){//same as creation of the toGiveArgs array
                int currSize;
                if(i==activeMonitors-1){
                    currSize = 10+int(countryList.count/activeMonitors);
                }else{
                    currSize = 10+int(countryList.count/activeMonitors)+temp;
                }
                for(int j=0;j<currSize;j++){
                    delete[] toGiveArgs[i][j];
                }
                delete[] toGiveArgs[i];
            }
            return 0;
        }
        cin.get();
        string line;
        getline(cin, line);
        string ctemp[8];//convert the readed string to a string array for more simplicity
        i=0;
        string word = "";
        for(auto x : line){
            if( x== ' '){
                ctemp[i] = word;
                i++;
                word = "";
            }else{
                word = word + x;
            }
        }
        ctemp[i] = word;
        i++;
        if(command == "/travelRequest"){
            travelRequest(&stats, &viruses, socketFds, socketBufferSize, activeMonitors, int(countryList.count/activeMonitors)+1, toGiveDirs, monitorPids, stoi(ctemp[0]), ctemp[1], ctemp[2], ctemp[3], ctemp[4]);
            cout << "Done!" << endl;
        }else if(command == "/travelStats"){
            if(i==4){//in case we have a country or not
                stats.getStats(ctemp[0], ctemp[1], ctemp[2], ctemp[3]);
                cout << "Done!" << endl;
            }else if(i==3){
                stats.getStats(ctemp[0], ctemp[1], ctemp[2]);
                cout << "Done!" << endl;
            }else{
                cout << "Error /travelStatus wrong input" << endl;
            }
        }else if(command == "/addVaccinationRecords"){
            addVaccinationRecords(socketFds, socketBufferSize, activeMonitors, int(countryList.count/activeMonitors)+1, toGiveDirs, monitorPids, ctemp[0], &viruses);
            cout << "Done!" << endl;
        }else if(command == "/searchVaccinationStatus"){
            searchVaccinationStatus(socketFds, socketBufferSize, activeMonitors, monitorPids, stoi(ctemp[0]));
            cout << "Done!" << endl;
        }
        cout << endl;
    }
}
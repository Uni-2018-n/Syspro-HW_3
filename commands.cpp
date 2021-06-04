#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include "commands.hpp"
#include "fromProjectOneAndTwo/fromProjectOne/generalList.hpp"
#include "funcs.hpp"

//functions for the monitor process
void generateLogFile(int numOfCountries, string countries[], int total, int accepted, int rejected){
    //generates a log_file.pid file with all the countries and all the accepted/rejected/total request counters
    ofstream file;
    file.open("log_file."+to_string(getpid()), fstream::out | fstream::trunc);//create the file
    if(file.fail()){
        perror("Child: failed to create log_file\n");
        return;
    }
    for(int i=0;i<numOfCountries;i++){
        file << countries[i] << endl;
    }
    file << "TOTAL TRAVEL REQUESTS " << total << endl;
    file << "ACCEPTED " << accepted << endl;
    file << "REJECTED " << rejected << endl;
    file.close();
}

int handlFunctionMonitor(int socke, int bufferSize, int currFunc, GlistHeader* main_list){
    switch(currFunc){//simple switch-case for each protocol message case
        case 101:{ //brackets because cpp dosent like creating variables inside cases
                   //101 protocol message for /travelRequest command
            int id = readSocketInt(socke, bufferSize);//read the citizenID
            int tempSize = readSocketInt(socke, bufferSize);//and the virus name
            string vir = readSocket(socke, tempSize, bufferSize);
            string temp = main_list->vaccineStatus(id, vir, false);//checks if the citizen is vaccinated(false means that we dont want to print something from this function) and returns "" if citizen not vaccinated or a date if vaccinated
            if(temp.length() != 0){//handle everything as planed if vaccinated or not
                writeSocketInt(socke, bufferSize, 1);
                writeSocketInt(socke, bufferSize, temp.length());
                writeSocket(socke, bufferSize, temp);
            }else{
                writeSocketInt(socke, bufferSize, 0);
            }
            return readSocketInt(socke, bufferSize);//read the 1 or 0 that indicated if the request was accepted or rejected
        }
        case 104:{//104 case is for /searchVaccinationStatus command
            int id = readSocketInt(socke, bufferSize);//read the citizenID
            SRListHeader* returnedViruses = main_list->vaccineStatus(id, false);//vaccineStatus function with boolean returns a SRListHeader list header with all the vaccinations that a citizen has done instead of printing messages
            if(returnedViruses != NULL && returnedViruses->len > 0){//check if the citizen exists
                writeSocketInt(socke, bufferSize, 1);//send 1 to indicate that the citizen exists here
                string temp = to_string(*(returnedViruses->citizen->citizenId)) + " " + returnedViruses->citizen->firstName + " " + returnedViruses->citizen->lastName + " " + returnedViruses->citizen->getCountry();//format the information string(id, name, country)
                writeSocketInt(socke, bufferSize, temp.length());
                writeSocket(socke, bufferSize, temp);
                writeSocketInt(socke, bufferSize, returnedViruses->citizen->getAge());//send the citizen's age
                writeSocketInt(socke, bufferSize, returnedViruses->len);//and send how many vaccinations the citizen has done
                for(int i=0;i<returnedViruses->len;i++){//for each vaccination
                    SRListNode* poped = returnedViruses->pop();
                    writeSocketInt(socke, bufferSize, poped->vir.length());//send the information to the parent(0 or 1 and date if 1)
                    writeSocket(socke, bufferSize, poped->vir);
                    writeSocketInt(socke, bufferSize, poped->vacced);
                    if(poped->vacced == 1){
                        writeSocketInt(socke, bufferSize, poped->date.length());
                        writeSocket(socke, bufferSize, poped->date);
                    }
                    delete poped;
                }
            }else{
                writeSocketInt(socke, bufferSize, 0);//else simple send 0 for the case that the citizen dosent exist
            }
            delete returnedViruses;
            return -1;
        }
        case 105:{//case EXIT
            return -2;
        }
    }
    return -1;
}
#ifndef _PARENTCOMMANDS_HPP_
#define _PARENTCOMMANDS_HPP_

#include "fromProjectOneAndTwo/fromProjectOne/Structures/virusesList.hpp"
#include "fromProjectOneAndTwo/fromProjectTwo/travelStatsList.hpp"

using namespace std;

void travelRequest(TSHeader* stats, VirlistHeader* viruses, int socketfds[], int bufferSize, int activeMonitors, int dirCount, string** toGiveDirs, int monitorPids[], int citizenID, string date, string countryFrom, string countryTo, string virusName);
void addVaccinationRecords(int socketfds[], int bufferSize, int activeMonitors, int dirCount, string** toGiveDirs, int monitorPids[], string country, VirlistHeader* viruses);
void searchVaccinationStatus(int socketfds[], int bufferSize, int activeMonitors, int monitorPids[], int citizenID);
void generateLogFileParent(int activeMonitors, int numOfCountries, string** countries, int total, int accepted, int rejected);

#endif
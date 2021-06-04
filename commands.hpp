#ifndef _COMMANDS_HPP_
#define _COMMANDS_HPP_

#include "fromProjectOneAndTwo/fromProjectOne/generalList.hpp"
using namespace std;

void generateLogFile(int numOfCountries, string countries[], int total, int accepted, int rejected);
int handlFunctionMonitor(int socke, int bufferSize, int currFunc, GlistHeader* main_list);
#endif
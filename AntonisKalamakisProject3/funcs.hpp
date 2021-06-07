#ifndef _FUNCS_HPP_
#define _FUNCS_HPP_

#include <iostream>

using namespace std;

int readSocketInt(int fd, int bufferSize);
string readSocket(int fd, int size, int bufferSize);
void writeSocketInt(int fd, int bufferSize, int t);
void writeSocket(int fd, int bufferSize, string txt);

#endif
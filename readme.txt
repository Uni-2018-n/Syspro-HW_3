Antonis Kalamakis SDI1800056

./travelMonitorClient –m numMonitors -b socketBufferSize -c cyclicBufferSize -s sizeOfBloom -i input_dir -t numThreads


Most of the things in this project are described in detail in the project 2 and project 1 readme files.

Minor changes in this project:
funcs.cpp/hpp:
Changed the name of the functions and removed the action if statements since we dont need signals.

commands.cpp/hpp:
Removed appendData function since we now have it in the main and added the 105 case in the handlFunctionMonitor function
for program termination.
Also all of the functions that previously had 2 fds as arguments now have one since we only have 1 socket for both way communication.

parentCommands.cpp/hpp:
Inside addVaccinationRecords function changed to parent sends protocol message from sockets instead of signal.
Also all of the functions that previously had 2 fds as arguments now have one since we only have 1 socket for both way communication.


travelMonitorClient.cpp:
In my implementation i've used execvp to run the ./monitorServer process after the fork command, for this i needed to 
make a toGiveArgs array. This array stores all the data that are going to be passed as arguments to the child processes.
The first 10 are the nessasary arguments and the rest of them are the country paths as provided by the user in the command line.

Used some of the code provided in this class to implement and start socket communication between parent and child processes and stored
the nessasary information needed for each monitor that we have up and running into arrays for later usage.
When done initializing and receiving the bloom filters from the monitors, we have the simple infinite while loop as used in previous projects, 
when the user sends an /exit command program sends to each active monitor a 105 protocol message to terminate and wait for it to terminate and ofcorse 
close all the nessasary structures. Finally generate a log file and delete all the allocated memory.

For monitorServer.cpp:
Used some code with some modifications provided in this class to create a cyclic buffer that is able to work with threads without having 
syncronization issues. poolMtx mutex is used to lock and unlock access in the shared data, poolFull condition variable is used for a thread to be able to wait
incase we have a full buffer and poolEmpty is for waiting incase we have a empty buffer.
main_list general list is now a global variable because of the fact that the other threads need to access it to insert new information and we have a dedicated mutex
for it so the other threads can read from the buffer while a thread inserts data into the list.
getCountry function simply receives a relative path for the country folder and returns the country so it will be possible for us (in case of addVaccinationRecords) to read
new added files. And threadFunction is the function that the threads are running.
During the initialization process because of the fact that we dont know how many paths we have as arguments there im using a pathToDirs string array with dirLen length to store them. 
After all the initialization of the variables,  condition variables etc, first we need to get the address of the system by passing the "localhost" address to the gethostbyname function, 
and finally open a socket and request connection to it for communicating with the parent process.
In this implementation im using new numThreads threads each time there is a request for reading files and appending data to the main_list and then the threads close. 
So after this im opening numThreads threads and the main thread starts opening each folder provided from the parent process from the command line, reads each file and inserts it into the 
cyclic buffer. When the parent thread inserted all the files into the buffer it sends one last message (one for each thread) "EXIT" indicating a protocol message saying that the insertion is done
so the threads can exit, and so the parent thread waits for the threads to exit and then continue to encode the bloom filters and send them to the parent process with sockets.
After all that the program continues into a infinite while loop waiting the parent process to send a protocol message for serving a command. 
There are 2 new protocol messages, 
106 is the message for addVaccinationRecords command and i've removed the function(that was before) and added into the main for keeping the handlFunctionMonitor function simpler.
Simply for addVaccinationRecords we start numThreads threads continue doing things as done in project 2 and as before send exit protocol message and wait for the threads to exit, after that it re-encodes the blooms
and send the blooms into the parent process as before.
The second new protocol message, is 105 indicating that the program has received a /exit command from the user, monitorServer generates a logfile, dealocated all nessasary memory and destroys all the mutexes and condition variables.
Finally for the thread function simply a while loop that reads each time one path from the cyclic buffer, waits if the buffer is empty and signals that the buffer is not full when it gets one path, checks if the message
just read is a protocol message EXIT, if yes breaks the loop and exits, if not locks the main_list mutex and inserts each line from the path just read into the main_list list.
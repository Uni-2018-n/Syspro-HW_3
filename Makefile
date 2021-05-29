FLAGS = -std=c++11 -Wall -g -o
SOURCE1 = travelMonitorClient.cpp fromProjectOneAndTwo/fromProjectTwo/sList.cpp
ITEM1 = travelMonitorClient
SOURCE2 = monitorServer.cpp
ITEM2 = monitorServer

compile : $(SOURCE)
	# gcc $(FLAGS) $(ITEM) $(SOURCE)
	g++ $(FLAGS) $(ITEM1) $(SOURCE1)
	g++ $(FLAGS) $(ITEM2) $(SOURCE2)

run : compile
	./$(ITEM)
	
mem : compile
	valgrind --leak-check=full ./$(ITEM) $(ARGS)

debug : $(ITEM)
	gdb $(ITEM)

clean :
	-rm $(ITEM)


CC = g++
CC_FLAGS = -std=c++11 -g
L_FLAGS = -lpthread
 
SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)


default: $(OBJECTS)
	$(CC) $(OBJECTS) $(L_FLAGS)
 
%.o: %.cpp
	$(CC) -c $(CC_FLAGS) $< -o $@
 
clean:
	rm -f ./a.out $(OBJECTS)
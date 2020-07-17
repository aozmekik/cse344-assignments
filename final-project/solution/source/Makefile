CXX = gcc
CXXFLAGS = -Wall -Werror -Wextra -Wpedantic -g 
LDFLAGS = 
LBLIBS = -lpthread -lm

SRC_SERVER = server.c utils.c utils.h queue.h graph.h cache.h
SRC_CLIENT = client.c utils.c utils.h
OBJ_SERVER = $(SRC_SERVER:.cc=.o)
OBJ_CLIENT = $(SRC_CLIENT:.cc=.o)
EXEC_SERVER = server
EXEC_CLIENT = client

all: $(EXEC_SERVER) $(EXEC_CLIENT)

$(EXEC_SERVER): $(OBJ_SERVER)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_SERVER) $(LBLIBS)

$(EXEC_CLIENT): $(OBJ_CLIENT)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_CLIENT) $(LBLIBS)

clean:
	
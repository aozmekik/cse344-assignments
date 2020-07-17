CXX = gcc
CXXFLAGS = -Wall -Werror -Wextra -Wpedantic -fsanitize=address -g 
LDFLAGS = 
LBLIBS = -lpthread

SRC = program.c utils.c utils.h
OBJ = $(SRC:.cc=.o)
EXEC = program

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LBLIBS)

clean:
	rm TEMP* output.txt
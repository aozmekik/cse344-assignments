CXX = gcc
CXXFLAGS = -Wall -Werror -Wextra -Wpedantic -fsanitize=address -g 
LDFLAGS = 
LBLIBS = -lpthread -lm

SRC = program.c utils.c utils.h queue.h model.h
OBJ = $(SRC:.cc=.o)
EXEC = program

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LBLIBS)

clean:
	rm TEMP* output.txt
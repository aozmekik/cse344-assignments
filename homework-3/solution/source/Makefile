CXX = gcc
CXXFLAGS = -Wall -Werror -Wextra -pedantic -g 
LDFLAGS = 
LBLIBS = -lm 

SRC = program.c utils.c svd.c
OBJ = $(SRC:.cc=.o)
EXEC = program

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LBLIBS)

clean:
	rm TEMP* output.txt
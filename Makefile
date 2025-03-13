PROGRAM=program
ARGS=
INC=-lncurses -ltinfo

default:
	gcc *.c $(INC) -o $(PROGRAM).o
                                     
strict:                              
	bear -- gcc *.c $(INC) -std=c99 -Wall -pedantic -Wextra -o $(PROGRAM).o
                    
debug:              
	gcc *.c $(INC) -std=c99 -Wall -pedantic -Wextra -g -o $(PROGRAM).o

run:
	./$(PROGRAM).o $(ARGS)

andrun:
	gcc *.c -o $(PROGRAM).o
	./$(PROGRAM).o $(ARGS)

gdb:
	gdb ./$(PROGRAM).o $(ARGS)

valgrind:
	valgrind -s --leak-check=yes --track-origins=yes ./$(PROGRAM).o $(ARGS)

clean:
	rm -f $(PROGRAM).o
	rm -f compile_commands.json


PROGRAM=program

default:
	gcc *.c -o $(PROGRAM).o
                                     
strict:                              
	bear -- gcc *.c -std=c99 -Wall -pedantic -Wextra -o $(PROGRAM).o
                    
debug:              
	gcc *.c -std=c99 -Wall -pedantic -Wextra -g -o0 -o $(PROGRAM).o

run:
	./$(PROGRAM).o

andrun:
	gcc *.c -o $(PROGRAM).o
	./$(PROGRAM).o

gdb:
	gdb ./$(PROGRAM).o

valgrind:
	valgrind -s --leak-check=yes --track-origins=yes ./$(PROGRAM).o

clean:
	rm -f $(PROGRAM).o
	rm -f compile_commands.json


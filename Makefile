default:
	gcc *.c -o program
                                     
strict:                              
	bear -- gcc *.c -std=c99 -Wall -pedantic -Wextra -o program
                    
debug:              
	gcc *.c -std=c99 -Wall -pedantic -Wextra -g -o0 -o program

run:
	./program

andrun:
	gcc *.c -o program
	./program

gdb:
	gdb ./program

valgrind:
	valgrind -s --leak-check=yes --track-origins=yes ./program

clean:
	rm -f program
	rm -f compile_commands.json


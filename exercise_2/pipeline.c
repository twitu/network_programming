#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define MAX_COMMANDS 5
#define MAX_ARGUMENTS 10

int main(int argc, char* argv[]) {
	if (argc != 2) exit(EXIT_FAILURE);
	int count, command_number, pipes, args;
	char* commands[MAX_COMMANDS][MAX_ARGUMENTS];

	// break commands into arguments
	count = 0;
	command_number = 0;
	args = 0;
	char* token = strtok(argv[1], " ");
	while (token != NULL) {
		if (strcmp(token, "|") == 0) {
			commands[command_number][args] = NULL;
			command_number++;
			args = 0;
		} else {
			char* storage = malloc(strlen(token)*sizeof(char));
			strcpy(storage, token);
			commands[command_number][args++] = storage;
		}
		token = strtok(NULL, " ");
	}
	pipes = command_number;
	commands[pipes][args] = NULL; // terminate last command with NULL
	command_number++;

	// pre declare pipes
	int filedes[pipes][2];
	int child, current_commmand = 0;
	while (1) {
		if (current_commmand < pipes) { // don't create pipe for last command
			if (pipe(filedes[current_commmand]) == -1) exit(EXIT_FAILURE);
			child = fork();
		} else {
			child = 1;
		}

		if (child == 0) { // child process
			close(0);
			dup(filedes[current_commmand][0]); // read end of pipe points to stdin
			close(filedes[current_commmand][1]); // close write end of pipe for child
			current_commmand++;
		} else if (child > 0) { // parent process
			if (current_commmand < pipes) { // if not last process
				close(1);
				dup(filedes[current_commmand][1]); // stdout points to write end of pipe
				close(filedes[current_commmand][0]); // close pointer to read end of pipe
			}
			execvp(commands[current_commmand][0], commands[current_commmand]); // execute current command
		} else {
			exit(EXIT_FAILURE);
		}
	}
}

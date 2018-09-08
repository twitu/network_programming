#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>

// length of global array
#define LENGTH 1000

// global variables
int global_array[LENGTH + 1];
int num_child;

// signal handlers
void handle_sigusr2() {kill(getppid(), SIGUSR2);}
void dummy_handler() {}
void handle_sigchld() {	exit(0);}

// stub
void begin_fork();

int main(int argc, char* argv[]) {
	if (argc!=2) exit(-1);
	num_child = atoi(argv[1]);

	printf("starting program in process %d\n", getpid());

	// populate the global array
	for (int i = 1; i <= LENGTH; i++) {
		global_array[i] = i;
	}

	// create signal masks for SIGUSR1 and SIGUSR2
	sigset_t blockset, prevmask;
        sigemptyset(&blockset);
        sigaddset(&blockset, SIGUSR1);
        sigaddset(&blockset, SIGUSR2);
	sigprocmask(SIG_BLOCK, &blockset, &prevmask); // block SIGUSR1, SIGUSR2 for all subsequent processes
	// handlers only for initializer process
	signal(SIGUSR1, dummy_handler);
	signal(SIGUSR2, dummy_handler);
	signal(SIGCHLD, handle_sigchld);

	int child = fork();
	if (child == 0) { // first child
		begin_fork(1, prevmask);
	} else if (child > 0) { // initalizer process
		while (1) {
			kill(child, SIGUSR1); // start traversing down process sequence
			sigsuspend(&prevmask);
		}
	} else {
		exit(-1);
	}
}

void begin_fork(int seq_id, sigset_t prevmask) {

	prctl(PR_SET_PDEATHSIG, SIGHUP); // receives SIGHUP signal when parent dies and kills itself
	sigset_t accept_sigusr1 = prevmask;
	sigset_t accept_sigusr2 = prevmask;
	sigaddset(&accept_sigusr1, SIGUSR2);
	sigaddset(&accept_sigusr2, SIGUSR1);

	// set signal handlers
	signal(SIGUSR1, dummy_handler);
	signal(SIGUSR2, handle_sigusr2);
	signal(SIGCHLD, handle_sigchld);

	if (seq_id < num_child) { // fork recursively
		int child = fork();
		if (child == 0) { // child process forks recursively
			begin_fork(++seq_id, prevmask);
		} else if (child > 0) { // current process waits for signal
			int index, itr = 0;
			while (1) {
				index = itr*(num_child) + seq_id;
				if (index <= LENGTH) {
					printf("process %d, value %d\n", getpid(), global_array[index]);
					sigsuspend(&accept_sigusr2); // print and wait for SIGUSR2 from child
					itr++;
				} else {
					exit(0);
				}
				sigsuspend(&accept_sigusr1);
				kill(child, SIGUSR1);
			}
		} else {
			exit(-1);
		}
	} else { // last process in the sequence
		int index, itr = 0;
		while (1) {
			index = itr*(num_child) + seq_id;
			if (index <= LENGTH) {
				printf("process %d, value %d\n", getpid(), global_array[index]);
				itr++;
				kill(getppid(), SIGUSR2); // last process, send SIGUSR2 up to parent
			} else {
				exit(0);
			}
			sigsuspend(&accept_sigusr1);
		}
	}
}

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/prctl.h>

int main(int argc, char* argv[]) {
    if (argc != 2) exit(EXIT_FAILURE);
    int n = atoi(argv[1]);
    int i;

    sem_t* mutex = (sem_t*) malloc(sizeof(sem_t)*n);
    for (i = 0; i < n; i++) {
        sem_init(&mutex[i], 1, 1);
    }

    pid_t child;
    int left, right, seq;
    for (i = 0; i < n; i++) {
        child = fork();
        if (child == 0) {
            // child process, initialize left and right index
            prctl(PR_SET_PDEATHSIG, SIGKILL);
            seq = i;
            // left fork is same as philosopher id
            left = i;
            // right fork id is one ahead of philospher id with wrap around
            if (seq == n-1) right = 0;
            else right = seq+1;
            printf("(process %d) philosopher %d is sitting at the table\n", getpid(), i+1);
            break;
        } else if (child > 0) {
            // parent process
            continue;
        } else {
            // error in forking, terminate process
            exit(-1);
        }
    }

    if (child == 0) {
        // child process should start eat think cycle
        child = getpid();
        while (1) {
            // even philosopher takes left fork first, while odd philospher takes right fork first
            if (seq % 2 == 0) {
                sem_wait(&mutex[left]);
                sem_wait(&mutex[right]);
            } else {
                sem_wait(&mutex[right]);
                sem_wait(&mutex[left]);
            }
            // critical section
            printf("(process %d) philosopher %d is eating\n", child, seq+1);
            // release fork after eating
            sem_post(&mutex[left]);
            sem_post(&mutex[right]);
        }
    } else {
        // parent process pauses and waits for ctrl-c
        pause();
    }
}

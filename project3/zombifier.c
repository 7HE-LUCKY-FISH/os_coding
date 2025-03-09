#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>

//global variables
int num_zombies = 0;
pid_t *zombie_pids = NULL;
int cleanup_complete = 0;

//SIGCONT handler
void handle_sigcont(int sig) {

    //clean up zombie processes
    for (int i = 0; i < num_zombies; i++) {
        int status;
        if (waitpid(zombie_pids[i], &status, 0) > 0) {
            printf("Cleaned up zombie %d (PID: %d)\n", i + 1, zombie_pids[i]);
        }
    }
    
    //free memory
    free(zombie_pids);

    cleanup_complete = 1;
}

void handle_pause(int sig){

}


int main(int argc, char *argv[]) {
    int opt;
    
    //command line with getopt
    while ((opt = getopt(argc, argv, "n:")) != -1) {
        switch (opt) {
            case 'n':
                num_zombies = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -n <number_of_zombies>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    
    if (num_zombies <= 0) {
        fprintf(stderr, "Please specify a positive number of zombies with -n option\n");
        fprintf(stderr, "Usage: %s -n <number_of_zombies>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    //allocate memory
    zombie_pids = (pid_t *)malloc(num_zombies * sizeof(pid_t));
    if (zombie_pids == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    //attach signal handler
    if (signal(SIGCONT, handle_sigcont) == SIG_ERR) {
        perror("signal");
        free(zombie_pids);
        exit(EXIT_FAILURE);
    }
    
    printf("Creating %d zombies\n", num_zombies);
    
    //making zombies
    for (int i = 0; i < num_zombies; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork");
            free(zombie_pids);
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            //child process
            printf("Child %d (PID: %d) is exiting and becoming a zombie\n", i + 1, getpid());
            exit(0);
        } else {
            //parent process
            zombie_pids[i] = pid;
            printf("Created zombie %d (PID: %d)\n", i + 1, pid);
        }
    }
    
    printf("%d zombies create\n", num_zombies);

    if(signal(SIGALRM, handle_pause) == SIG_ERR){
        perror("signal");
        free(zombie_pids);
        exit(EXIT_FAILURE);
    }
    alarm(2);
    pause();  

    //send SIGCONT to all zombies and then clean them up
    printf("Sending SIGCONT to all zombies\n");
    for (int i = 0; i < num_zombies; i++) {
        //printf("SIGCONT: zombie %d, PID %d\n", i + 1, zombie_pids[i]);
        kill(zombie_pids[i], SIGCONT);
    }
    
    //send SIGCONT to self to trigger the cleanup handler
    printf("self cleanup PID: %d\n", getpid());
    kill(getpid(), SIGCONT);
    
    //wait for cleanup to complete
    while (!cleanup_complete) {
        sleep(1);
    }
    
    return 0;
}
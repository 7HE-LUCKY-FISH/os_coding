#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

void sigint_handler(int sig){

}

int main(void) {
    signal (SIGINT, SIG_IGN);//ignores the signal so it doesn't kill the parent process as well
    pid_t pid = fork();//create a child process
    
    if (pid < 0) { 
        fprintf(stderr, "Fork Failed");//check if the fork has failed
        exit(EXIT_FAILURE);//check if the fork has failed and exits with error 
    }
    
    if (pid == 0) {
        signal(SIGINT, sigint_handler);//signal handler for the child process
        printf("Child Process PID: %d\n", getpid()); 
        fflush(stdout);
        pause();
        exit(5);
    } else {
        int status;
        pid_t child_pid = wait(&status);//waits for the child process to finish
        int exitstatus = 0;


       if (WIFEXITED(status)) {//checks the exit status of the child process
            exitstatus = WEXITSTATUS(status);
            printf("childpid=%d,exitstatus=%d\n", child_pid, exitstatus);
        }
        else {
            fprintf(stderr, "childpid=%d terminated abnormally\n", child_pid);
            return EXIT_FAILURE;//if the child process was terminated abnormally
        }
    }
    return 0;
}
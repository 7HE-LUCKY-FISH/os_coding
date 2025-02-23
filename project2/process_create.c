#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

void sigint_handler(int sig){

}

int main(void) {
    pid_t pid = fork();
    
    if (pid < 0) { 
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0) {
        signal(SIGINT, sigint_handler);
        printf("Child Process PID: %d\n", getpid());
        pause();
        exit(5);
    } else {
        int status;
        sleep(1);   
        kill(pid, SIGINT);


        pid_t child_pid = wait(&status);
        int exitstatus = 0;
        if (WIFEXITED(status)) {
            exitstatus = WEXITSTATUS(status);
        }
        printf("childpid = %d,exitstatus=%d\n", child_pid, exitstatus);
    }
    
    return 0;
}
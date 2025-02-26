#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

void sigint_handler(int sig){

}

int main(void) {
    pid_t pid = fork();//create a child process
    
    if (pid < 0) { 
        exit(EXIT_FAILURE);//check if the fork has failed and exits with error 
    }
    
    if (pid == 0) {
        signal(SIGINT, sigint_handler);//signal handler for the child process
        //printf("Child Process PID: %d\n", getpid()); debug statment 
        pause();
        exit(5);
    } else {
        int status;
        kill(pid, SIGINT); //sends a signal to the child process


        pid_t child_pid = wait(&status);//waits for the child process to terminate and grabs pid
        int exitstatus = 0;
       if (WIFEXITED(status)) {//checks the exit status of the child process
            exitstatus = WEXITSTATUS(status);
            printf("childpid=%d,exitstatus=%d\n", child_pid, exitstatus);
        } else if (WIFSIGNALED(status)) {
            printf("childpid=%d terminated by signal %d\n", //checks if the child process was terminated by a signal
                child_pid, WTERMSIG(status));
            return EXIT_FAILURE;
        } else {
            printf("childpid=%d terminated abnormally\n", child_pid);
            return EXIT_FAILURE;//if the child process was terminated abnormally
        }
    }
    
    return 0;
}
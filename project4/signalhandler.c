#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <error.h>
#include <stdlib.h>

static bool debug_mode = false;
static int count = 1;

void handle_sigint(int sig){
    debug_mode = !debug_mode; 
    printf("SIGINT received. Debug mode is now %s.\n", debug_mode ? "ON" : "OFF");

}

void handle_sigusr1(int sig) {
    printf("SIGUSR1 received.\n");
    exit(0);  // Terminate with status 0
}

int main(){
    if (signal(SIGINT, handle_sigint) == SIG_ERR) {
        perror("Failed to set SIGINT handler");
        return 1;
    }
    
    if (signal(SIGUSR1, handle_sigusr1) == SIG_ERR) {
        perror("Failed to set SIGUSR1 handler");
        return 1;
    }

    while(1){
        if(debug_mode){
            printf("Iteration %d: Debug mode is ON\n", count);
            count++;
        }
        sleep(2);
    }

    return 0;
}
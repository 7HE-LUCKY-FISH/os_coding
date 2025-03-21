#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <error.h>
#include <stdlib.h>

static bool debug_mode = false;
static int count = 1;

//this is to toggle the debug_mode on and off with the sigint signal
void handle_sigint(int sig){
    debug_mode = !debug_mode; 
    printf("SIGINT received. Debug mode is now %s.\n", debug_mode ? "ON" : "OFF");

}

//if we get a sigusr1 we terminate the program 
void handle_sigusr1(int sig) {
    printf("SIGUSR1 received.\n");
    exit(0);  // Terminate with status 0
}

int main(){
    //attach the signal if there is a error return 
    if (signal(SIGINT, handle_sigint) == SIG_ERR) {
        perror("Failed to set SIGINT handler");
        return 1;
    }
    //same thing as above
    if (signal(SIGUSR1, handle_sigusr1) == SIG_ERR) {
        perror("Failed to set SIGUSR1 handler");
        return 1;
    }
    //infinite loop to keep on checking for the signal we print if there is a change in the sigint
    //if there isn't we just sleep and not check 
    while(1){
        //if debug is on we print and increment the count
        if(debug_mode){
            printf("Iteration %d: Debug mode is ON\n", count);
            count++;
        }
        //requiremnt for the sleep 
        sleep(2);
    }

    return 0;
}
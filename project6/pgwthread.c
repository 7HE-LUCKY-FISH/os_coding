#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

// Global variables for thread synchronization
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER;
volatile int thread1_go = 1; // Initially thread 1 starts
volatile int thread2_go = 0;
volatile int running = 1; // Flag to control thread execution

// Signal handler for SIGINT
void handle_sigint(int sig) {
    running = 0;
    // Signal both threads to check running state
    pthread_cond_signal(&cond1);
    pthread_cond_signal(&cond2);
}

// Thread 1 function
void *thread1_func(void *arg) {
    while (running) {
        pthread_mutex_lock(&mutex);
        // Wait until it's thread 1's turn or program is stopping
        while (!thread1_go && running) {
            pthread_cond_wait(&cond1, &mutex);
        }
        
        if (!running) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        
        // First output - ping
        printf("thread 1: ping thread 2\n");
        fflush(stdout);
        
        // Signal thread 2 to go
        thread1_go = 0;
        thread2_go = 1;
        pthread_cond_signal(&cond2);
        
        // Wait for thread 2 to signal back
        while (!thread1_go && running) {
            pthread_cond_wait(&cond1, &mutex);
        }
        
        if (!running) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        
        // Second output - pong response
        printf("thread 1: pong! thread 2 ping received\n");
        fflush(stdout);
        
        pthread_mutex_unlock(&mutex);
    }
    
    return NULL;
}

// Thread 2 function
void *thread2_func(void *arg) {
    while (running) {
        pthread_mutex_lock(&mutex);
        // Wait until it's thread 2's turn or program is stopping
        while (!thread2_go && running) {
            pthread_cond_wait(&cond2, &mutex);
        }
        
        if (!running) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        
        // First output - pong response
        printf("thread 2: pong! thread 1 ping received\n");
        fflush(stdout);
        
        // Second output - ping
        printf("thread 2: ping thread 1\n");
        fflush(stdout);
        
        // Signal thread 1 to go
        thread2_go = 0;
        thread1_go = 1;
        pthread_cond_signal(&cond1);
        pthread_mutex_unlock(&mutex);
    }
    
    return NULL;
}

int main() {
    pthread_t thread1, thread2;
    int rc;
    // Set up signal handler using basic signal() function
    if (signal(SIGINT, handle_sigint) == SIG_ERR) {
        fprintf(stderr, "Cannot set signal handler\n");
        return EXIT_FAILURE;
    }
    
    // Create threads
    rc = pthread_create(&thread1, NULL, thread1_func, NULL);
    if(rc !=0){
        fprintf(stderr, "Error creating thread 1: %d\n", rc);
        exit(EXIT_FAILURE);
    }
    rc = pthread_create(&thread2, NULL, thread2_func, NULL);
    if (rc != 0) {
        fprintf(stderr, "Error creating thread 2: %d\n", rc);
        exit(EXIT_FAILURE);
    }
    // Wait for threads to complete (which will happen when SIGINT is received)
    rc = pthread_join(thread1, NULL);
    if (rc != 0) {
        fprintf(stderr, "Error joining thread 1: %d\n", rc);
        return EXIT_FAILURE;
    }
    
    rc = pthread_join(thread2, NULL);
    if (rc != 0) {
        fprintf(stderr, "Error joining thread 2: %d\n", rc);
        return EXIT_FAILURE;
    }
    
    // Clean up resources
    if ((rc = pthread_mutex_destroy(&mutex)) != 0) {
        fprintf(stderr, "Error destroying mutex: %d\n", rc);
        return EXIT_FAILURE;
    }
    
    if ((rc = pthread_cond_destroy(&cond1)) != 0) {
        fprintf(stderr, "Error destroying condition variable: %d\n", rc);
        return EXIT_FAILURE;
    }
    if ((rc = pthread_cond_destroy(&cond2)) != 0) {
        fprintf(stderr, "Error destroying condition variable: %d\n", rc);
        return EXIT_FAILURE;
    }
    return 0;
}
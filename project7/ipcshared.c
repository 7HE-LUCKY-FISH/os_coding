#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define BUFFER_SIZE 2048
#define SOCKET_PATH "/tmp/producer_consumer_socket"
#define SHM_NAME "/producer_consumer_shm"


/*
rm -f /dev/shm/sem.producer_consumer_mutex
rm -f /dev/shm/sem.producer_consumer_full
rm -f /dev/shm/sem.producer_consumer_empty

current running 
./ipcshared -p -m "hello world" -q 5 -u -e
./ipcshared -c -m "hello world" -q 5 -u -e


./ipcshared -p -m "hello world" -q 5 -s -e
./ipcshared -c -m "hello world" -q 5 -s -e


*/


sem_t *full, *empty, *mutex;
char *buffer;
int queue_size = 10;
int is_echo = 0;

typedef struct {
    int in;
    int out;
    char data[];
} shared_buffer;

shared_buffer *shm_buffer;


void cleanup_semaphores() {
    sem_unlink("/producer_consumer_mutex");
    sem_unlink("/producer_consumer_full");
    sem_unlink("/producer_consumer_empty");
}


void handle_unix_socket(int is_producer, char *message) {
    struct sockaddr_un addr;
    int sock_fd;
    
    // Create socket
    if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (is_producer) {
        // Producer code
        // Remove socket if it already exists
        unlink(SOCKET_PATH);
        
        // Bind socket to address
        if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            perror("bind error");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }
        
        // Listen for connections
        if (listen(sock_fd, 1) == -1) {
            perror("listen error");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }
        
        printf("Producer waiting for consumer connection...\n");
        int client_fd;
        if ((client_fd = accept(sock_fd, NULL, NULL)) == -1) {
            perror("accept error");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }
        
        // Send message queue_size times
        for (int i = 0; i < queue_size; i++) {
            sem_wait(empty);
            sem_wait(mutex);
            
            // Send message
            if (send(client_fd, message, strlen(message) + 1, 0) == -1) {
                perror("send error");
                close(client_fd);
                close(sock_fd);
                exit(EXIT_FAILURE);
            }
            
            if (is_echo) {
                printf("Produced: %s\n", message);
            }
            
            sem_post(mutex);
            sem_post(full);
            sleep(1); // Slow down for demonstration purposes
        }
        
        close(client_fd);
    } else {
        // Consumer code
        // Connect to socket
        if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            perror("connect error");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }
        
        // Receive messages
        char buffer[BUFFER_SIZE];
        for (int i = 0; i < queue_size; i++) {
            sem_wait(full);
            sem_wait(mutex);
            
            // Receive message
            int bytes_received = recv(sock_fd, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0) {
                if (bytes_received < 0) {
                    perror("recv error");
                }
                break;
            }
            
            buffer[bytes_received] = '\0';
            if (is_echo) {
                printf("Consumed: %s\n", buffer);
            }
            
            sem_post(mutex);
            sem_post(empty);
        }
    }
    
    close(sock_fd);
    if (is_producer) {
        unlink(SOCKET_PATH);
    }
}

void handle_shared_memory(int is_producer, char *message) {
    int shm_fd;
    
    // Create/open shared memory
    if ((shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666)) == -1) {
        perror("shm_open error");
        exit(EXIT_FAILURE);
    }
    /*
    // Set size of shared memory object
    if (ftruncate(shm_fd, sizeof(shared_buffer)) == -1) {
        perror("ftruncate error");
        exit(EXIT_FAILURE);
    }
    */
    size_t shm_size = sizeof(shared_buffer)  + (BUFFER_SIZE * queue_size);
    if (ftruncate(shm_fd, shm_size) == -1) {
        perror("ftruncate error");
        exit(EXIT_FAILURE);
    }
    
    // Map shared memory object into memory
    shm_buffer = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_buffer == MAP_FAILED) {
        perror("mmap error");
        exit(EXIT_FAILURE);
    }
    
    if (is_producer) {
        // Initialize buffer pointers
        shm_buffer->in = 0;
        shm_buffer->out = 0;
        
        // Produce messages
        for (int i = 0; i < queue_size; i++) {
            sem_wait(empty);
            sem_wait(mutex);
            
            // Add message to buffer
            strncpy(&shm_buffer->data[shm_buffer->in * BUFFER_SIZE], 
                    message, BUFFER_SIZE - 1);
            shm_buffer->in = (shm_buffer->in + 1) % queue_size;
            
            if (is_echo) {
                printf("Produced: %s\n", message);
            }
            
            sem_post(mutex);
            sem_post(full);
            sleep(1); // Slow down for demonstration purposes
        }
    } else {
        // Consumer code
        for (int i = 0; i < queue_size; i++) {
            sem_wait(full);
            sem_wait(mutex);
            
            // Consume message from buffer
            char consumed_msg[BUFFER_SIZE];
            strncpy(consumed_msg, &shm_buffer->data[shm_buffer->out * BUFFER_SIZE], 
                   BUFFER_SIZE - 1);
            consumed_msg[BUFFER_SIZE - 1] = '\0';
            shm_buffer->out = (shm_buffer->out + 1) % queue_size;
            
            if (is_echo) {
                printf("Consumed: %s\n", consumed_msg);
            }
            
            sem_post(mutex);
            sem_post(empty);
        }
    }
    
    // Clean up
    munmap(shm_buffer, shm_size);
    close(shm_fd);
    
    if (is_producer) {
        shm_unlink(SHM_NAME);
    }
}

int main(int argc, char *argv[]) {
    int opt;
    char *message = NULL;
    int is_producer = 0;
    int use_unix_socket = 0;
    
    // Fixed getopt string - removed colon after u
    while ((opt = getopt(argc, argv, "pcm:q:use")) != -1) {
        switch (opt) {
            case 'p':
                is_producer = 1;
                break;
            case 'c':
                is_producer = 0;
                break;
            case 'm':
                message = optarg;
                break;
            case 'q':
                queue_size = atoi(optarg);
                break;
            case 'u':
                use_unix_socket = 1;
                break;
            case 's':
                use_unix_socket = 0;
                break;
            case 'e':
                is_echo = 1;
                break;
            default:
                fprintf(stderr, "usage: %s [-p | -c] [-m message] [-q size] [-u | -s] [-e]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    
    if (!message) {
        fprintf(stderr, "no message provided.\n");
        exit(EXIT_FAILURE);
    }

    if(is_producer){
        cleanup_semaphores();
    }

    
    // Create named semaphores for sharing between processes
    mutex = sem_open("/producer_consumer_mutex", O_CREAT, 0666, 1);
    full = sem_open("/producer_consumer_full", O_CREAT, 0666, 0);
    empty = sem_open("/producer_consumer_empty", O_CREAT, 0666, queue_size);
    
    if (mutex == SEM_FAILED || full == SEM_FAILED || empty == SEM_FAILED) {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }
    
    if (use_unix_socket) {
        handle_unix_socket(is_producer, message);
    } else {
        handle_shared_memory(is_producer, message);
    }
    
    // Close and clean up semaphores
    sem_close(mutex);
    sem_close(full);
    sem_close(empty);
    
    if (is_producer) {
        sleep(1);
        // Producer cleans up semaphores
        sem_unlink("/producer_consumer_mutex");
        sem_unlink("/producer_consumer_full");
        sem_unlink("/producer_consumer_empty");
        cleanup_semaphores();
    }
    
    return 0;
}
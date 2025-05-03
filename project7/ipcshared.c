#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <errno.h>
#include <getopt.h>
#include <sys/select.h>  
#include <sys/time.h> 


//constant definitions
#define SOCKET_NAME "/tmp/pc.sock"
#define BUFFER_SIZE 1024
#define SHM_NAME "/pc_shm"
#define SEM_FULL "/sem_full"
#define SEM_EMPTY "/sem_empty"
#define SEM_MUTEX "/sem_mutex"

sem_t *full;
sem_t *empty;
sem_t *mutex;

//queue struct to store messages and manage producer-consumer shared memory
typedef struct
{
    int head;
    int tail;
    int q_size;
    int count;
    int running;
    int done;
    char messages[BUFFER_SIZE];
}queue_t;

queue_t *q_t;


//producer function for unix sockets
void producer_socket(bool e, const char *m, int q)
{
    struct sockaddr_un addr;
    for(int i = 0; i < q; i++)
    {
        int producer_file;
        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, SOCKET_NAME, sizeof(addr.sun_path) - 1);
        char buffer[BUFFER_SIZE];
        //loop for connection attempt if producer is ran first to retry connection until consumer
        while (1) 
        {
            producer_file = socket(AF_UNIX, SOCK_STREAM, 0);
            if (producer_file < 0) 
            {
                perror("Producer: socket failed");
                exit(EXIT_FAILURE);
            }

            //connect to consumer
            if(connect(producer_file, (const struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1)
            {
                perror("Connect failed, waiting for consumer");
                //sleep(1) so while its waiting to connect to consumer, it doesn't spam the terminal
                sleep(1);
                close(producer_file);
            }
            //if connected, break out of retry loop
            else
            {
                break;
            }
        }

        //send message
        if(write(producer_file, m, strlen(m)) < 0)
        {
            perror("Write failed");
            close(producer_file);
            exit(EXIT_FAILURE);
        }
        //if e argument is passed by user
        if(e)
        {
            printf("Message from Producer: %s\n", m);
        }
        close(producer_file);
  }
}

//consumer function for unix sockets
void consumer_socket(bool e, int q)
{
    int producer_file, con_fd;
    struct sockaddr_un addr;
    char buffer[BUFFER_SIZE];
    int messages_received = 0;
    struct timeval timeout;
    fd_set readfds;
    
    // socket creation
    if((producer_file = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    // set socket address
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, SOCKET_NAME, sizeof(addr.sun_path) - 1);
    
    unlink(SOCKET_NAME);

    if(bind(producer_file, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)))
    {
        perror("Bind failed");
        close(producer_file);
        exit(EXIT_FAILURE);
    }

    if(listen(producer_file, 5) == -1)
    {
        perror("Listen failed");
        close(producer_file);
        exit(EXIT_FAILURE);
    }
    
    printf("Consumer started. Waiting for messages...\n");
    
    // Run until timeout indicates no more producers
    int consecutive_timeouts = 0;
    while(1)
    {
        // Set up for select call
        FD_ZERO(&readfds);
        FD_SET(producer_file, &readfds);
        
        // Set timeout to 3 seconds
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(producer_file + 1, &readfds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            perror("Select error");
            break;
        }
        else if (activity == 0) {
            // Timeout occurred, no connection request
            consecutive_timeouts++;
            
            // If we've waited for multiple timeouts and already received some messages,
            // assume all producers are done
            if (consecutive_timeouts >= 2 && messages_received > 0) {
                printf("No new connections for %d seconds. Assuming all producers finished.\n", 
                       consecutive_timeouts * 3);
                break;
            }
            continue;
        }
        
        // Reset timeout counter if we get activity
        consecutive_timeouts = 0;
        
        // Now it's safe to accept
        con_fd = accept(producer_file, NULL, NULL);
        if(con_fd == -1)
        {
            perror("Accept failed");
            close(producer_file);
            exit(EXIT_FAILURE);
        }
        
        memset(buffer, 0, BUFFER_SIZE);
    
        if(read(con_fd, buffer, BUFFER_SIZE - 1) > 0)
        {
            messages_received++;
            if(e)
            {
                printf("Consumer received: %s (message %d)\n", buffer, messages_received);
            }
        }
        else
        {
            perror("Read failed");
        }
        
        close(con_fd);
    }

    printf("All messages consumed (%d total). Exiting.\n", messages_received);
    close(producer_file);
    unlink(SOCKET_NAME);
}


//function to create section of shared memory
void create_sharedmem(int q)
{
    int shm_fd = shm_open(SHM_NAME, O_CREAT| O_RDWR, 0666);
    if (shm_fd == -1) 
    {
        perror("shm_open failed");
        exit(EXIT_FAILURE);
    }
    
    struct stat shm_stat;
    if (fstat(shm_fd, &shm_stat) == -1) {
        perror("fstat failed");
        exit(EXIT_FAILURE);
    }
    
    int existing_q = 0;
    size_t needed_size = sizeof(queue_t) + (q * BUFFER_SIZE);
    size_t existing_size = shm_stat.st_size;
    
    // If shared memory already exists, determine its queue size
    if (existing_size > sizeof(queue_t)) {
        queue_t *temp = mmap(NULL, sizeof(queue_t), PROT_READ, MAP_SHARED, shm_fd, 0);
        if (temp == MAP_FAILED) {
            perror("mmap failed during size check");
            exit(EXIT_FAILURE);
        }
        existing_q = temp->q_size;
        munmap(temp, sizeof(queue_t));
        
        // Update needed size to use the larger of the two queue sizes
        int max_q = (existing_q > q) ? existing_q : q;
        needed_size = sizeof(queue_t) + (max_q * BUFFER_SIZE);
    }
    
    // Resize shared memory to accommodate the larger queue if needed
    if (needed_size > existing_size) {
        if (ftruncate(shm_fd, needed_size) == -1) {
            perror("ftruncate failed");
            exit(EXIT_FAILURE);
        }
    }

    // Map the shared memory
    q_t = mmap(NULL, needed_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (q_t == MAP_FAILED) 
    {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    
    sem_wait(mutex);
    if(q_t->running == 0){
        q_t->head = 0;
        q_t->tail = 0;
        q_t->q_size = q;
        q_t->count = 0;
        q_t->running = 1;
        q_t->done = 0;
        for (int i = 0; i < q; i++) 
        {
            memset(&q_t->messages[i * BUFFER_SIZE], 0, BUFFER_SIZE);
        }
    }
    else if (q > q_t->q_size) {
        // Expand the queue size if needed
        printf("Expanding queue from %d to %d\n", q_t->q_size, q);
        
        // Initialize new message slots
        for (int i = q_t->q_size; i < q; i++) 
        {
            memset(&q_t->messages[i * BUFFER_SIZE], 0, BUFFER_SIZE);
        }
        
        q_t->q_size = q;
    }
    
    q_t->count++;
    sem_post(mutex);
}

//function for producer in shared memory, iterates through queue size and produces messages 
void producer_shared(const char *m, int q, bool e){
    for(int i = 0; i < q; i++){
        sem_wait(empty);
        sem_wait(mutex);

        strncpy(&q_t->messages[q_t->head * BUFFER_SIZE], m, BUFFER_SIZE - 1);
        q_t->messages[q_t->head * BUFFER_SIZE + BUFFER_SIZE - 1] = '\0';
        q_t->head = (q_t->head +1) % q_t->q_size;
        if (e) 
        {
            printf("Message from Producer: %s\n", m);
        }
        sem_post(mutex);
        sem_post(full);

    }
    sem_wait(mutex);
    q_t->done = 1;
    sem_post(mutex);
}

//function for consumer in shared memory, continuously consumes messages
void consumer_shared(int q, bool e){
    printf("Consumer started. Waiting for messages...\n");
    
    while(1){
        // Check if producer is done and queue is empty
        sem_wait(mutex);
        int done = q_t->done;
        int full_val;
        sem_getvalue(full, &full_val);
        sem_post(mutex);
        
        if (done && full_val == 0) {
            printf("All messages consumed. Exiting.\n");
            break;
        }
        
        // Try to get a message without blocking
        if (sem_trywait(full) == 0) {
            // Successfully got a message
            sem_wait(mutex);
            char m[BUFFER_SIZE];

            strncpy(m, &q_t->messages[q_t->tail * BUFFER_SIZE], BUFFER_SIZE - 1);

            m[BUFFER_SIZE - 1] = '\0';
            q_t->tail = (q_t->tail + 1) % q_t->q_size;
            if (e) 
            {
                printf("Consumer Received: %s\n", m);
            }
            sem_post(mutex);
            sem_post(empty);
        } else {
            // No message available now, wait a bit before checking again
            usleep(100000); // 100ms
        }
    }
}

//function to cleanup semaphores after program runs 
void cleanup(){
    // Check if q_t is initialized (only happens in shared memory mode)
    if (q_t != NULL) {
        sem_wait(mutex);
        q_t->count--;
        int should_clean = (q_t->count == 0);
        int q_size = q_t->q_size;  // Save q_size before unmapping
        sem_post(mutex);

        if (should_clean) {
            printf("Cleaning up shared memory resources...\n");
            size_t total_size = sizeof(queue_t) + (q_size * BUFFER_SIZE);
            munmap(q_t, total_size);
            shm_unlink(SHM_NAME);
            
            // Also unlink semaphores since we're the last process
            sem_unlink(SEM_FULL);
            sem_unlink(SEM_EMPTY);
            sem_unlink(SEM_MUTEX);
        } else {
            // Just unmap our view of the shared memory
            size_t total_size = sizeof(queue_t) + (q_size * BUFFER_SIZE);
            munmap(q_t, total_size);
        }
        
        // Close the semaphores in any case
        sem_close(full);
        sem_close(empty);
        sem_close(mutex);
    } else {
        // For unix socket mode, close and unlink semaphores
        sem_close(full);
        sem_close(empty);
        sem_close(mutex);
        sem_unlink(SEM_FULL);
        sem_unlink(SEM_EMPTY);
        sem_unlink(SEM_MUTEX);
    }
}



int main(int argc, char *argv[]){
    //flags to keep track of cli arguments 
    bool is_producer = false;
    bool is_consumer = false;
    bool exist_msg = false;
    bool queue_arg = false;
    bool u_arg = false;
    bool s_arg = false;
    bool e_arg = false;
    char c;
    int q_depth = 10; // default queue depth
    char msg[BUFFER_SIZE] = {0};
    while((c =getopt(argc, argv, "pcm:q:use")) != -1){
        switch(c){
            case 'p':
                if(is_producer){
                  fprintf(stderr, "Error: Multiple -p Arguements Passed");
                  exit(EXIT_FAILURE);
                }
                is_producer = true;
                break;

            case 'c':
                if(is_consumer){
                  fprintf(stderr, "Error: Multiple -c Arguements Passed");
                  exit(EXIT_FAILURE);
                }
                is_consumer = true;
                break;

            case 'q':
                if(queue_arg){
                  fprintf(stderr, "Error: Multiple Arguments Passed");
                  exit(EXIT_FAILURE);
                }
                queue_arg = true;
                q_depth = atoi(optarg);
                break;

            case 'u':
                if(u_arg){
                    fprintf(stderr, "Error: Multiple -u Arguments Passed\n");
                    exit(EXIT_FAILURE);
                }
                u_arg = true;
                break;
            case 's':
                if(s_arg){
                    fprintf(stderr, "Error: Multiple -s Arguments Passed\n");
                    exit(EXIT_FAILURE);
                }
                s_arg = true;

                break;
            case 'e':
                if(e_arg){
                    fprintf(stderr, "Error: Multiple -e Arguments Passed \n");
                    exit(EXIT_FAILURE);
                }
                e_arg = true;
                break;
            
            case 'm':
                if(exist_msg){
                  fprintf(stderr, "Error: Multiple -m  Arguements Passed");
                  exit(EXIT_FAILURE);
                }
                exist_msg = true;

                strncpy(msg, optarg, BUFFER_SIZE - 1);
                break;
            default:
                fprintf(stderr, "Usage: %s -p/-c -q <depth> -u/-s -e -m <message>\n ", argv[0]);

        }
    }
    
    //error handling for aguments passed
    if ((is_producer && is_consumer) || (!is_producer && !is_consumer) ){
        fprintf(stderr, "Error: Please enter either -p or -c\n");
        exit(EXIT_FAILURE);
    }
    if ((u_arg && s_arg) || (!u_arg && !s_arg)){
        fprintf(stderr, "Error: Please enter either -u or -s\n");
        exit(EXIT_FAILURE);
    }
    
    // Queue size is required for producers
    if (is_producer && !queue_arg) {
        fprintf(stderr, "Error: Producer requires -q <depth>\n");
        exit(EXIT_FAILURE);
    }
    
    //create semaphores
    full = sem_open(SEM_FULL, O_CREAT, 0666, 0);
    empty = sem_open(SEM_EMPTY, O_CREAT, 0666, q_depth); 
    mutex = sem_open(SEM_MUTEX, O_CREAT, 0666, 1);


    if (empty == SEM_FAILED && errno == EEXIST) {
        empty = sem_open("/my_semaphore", 0); // Open existing
    }

    if (mutex == SEM_FAILED || full == SEM_FAILED || empty == SEM_FAILED) {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }

    //producer for unix socket
    if(is_producer && u_arg) {
        if(!exist_msg){
            fprintf(stderr, "Error: -p requires -m \n");
            exit(EXIT_FAILURE);
        }
        producer_socket(e_arg, msg, q_depth);
        cleanup();

    }
    //consumer for unix socket
    if(is_consumer && u_arg){
        consumer_socket(e_arg,q_depth);
    }
    
    //shared memory creation for producer
    if(s_arg && is_producer){
        create_sharedmem(q_depth);
    }

    //producer for shared memory
    if(is_producer && s_arg){
        if(!exist_msg){
            fprintf(stderr, "Error: -p requires -m\n ");
            exit(EXIT_FAILURE);
        }
        create_sharedmem(q_depth);
        producer_shared(msg, q_depth, e_arg);
        
        // Only close the semaphores but don't unlink them
        sem_close(full);
        sem_close(empty);
        sem_close(mutex);
        
        printf("Producer finished. Start consumer to process the data.\n");
        return 0; // Exit without calling cleanup()
    }
    
    //consumer for shared memory
    if(is_consumer && s_arg) {
        int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("Consumer: shm_open failed. Make sure a producer has created the shared memory");
            exit(EXIT_FAILURE);
        }
        
        // Get the existing queue size from shared memory
        queue_t *temp = mmap(NULL, sizeof(queue_t), PROT_READ, MAP_SHARED, shm_fd, 0);
        if (temp == MAP_FAILED) {
            perror("Consumer: mmap failed");
            exit(EXIT_FAILURE);
        }
        
        // Use the queue size from shared memory
        q_depth = temp->q_size;
        munmap(temp, sizeof(queue_t));
        
        create_sharedmem(q_depth);
        consumer_shared(q_depth, e_arg);
        cleanup(); // Only consumer does full cleanup
    }
    
    // Only call cleanup here for unix socket mode
    else if (u_arg) {
        cleanup();
    }
    
    return 0;
}


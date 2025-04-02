#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    int pipe_parent_to_child[2]; // pipe for parent to child communication
    int pipe_child_to_parent[2]; // pipe for child to parent communication
    pid_t pid;
    char buffer[100];
    
    // Create both pipes
    if (pipe(pipe_parent_to_child) == -1 || pipe(pipe_child_to_parent) == -1) {
        perror("pipe creation failed");
        exit(EXIT_FAILURE);
    }
    
    // forking process
    pid = fork();
    
    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    
    if (pid > 0) {
        // close unused pipe ends
        // 0 is read end 
        // 1 is write end
        close(pipe_parent_to_child[0]); // close read end of parent->child pipe
        close(pipe_child_to_parent[1]); // close write end of child->parent pipe
        
        // prepare and send message to child
        snprintf(buffer, sizeof(buffer), "I am your daddy! and my name is %d\n", getpid());
        write(pipe_parent_to_child[1], buffer, strlen(buffer) + 1);
        
        // wait for message from child
        read(pipe_child_to_parent[0], buffer, sizeof(buffer));
        printf("%s\n", buffer);
        
        // close remaining pipe ends
        close(pipe_parent_to_child[1]);
        close(pipe_child_to_parent[0]);
        
        // wait for child to exit to prevent zombie process
        int status;
        waitpid(pid, &status, 0);
        
    } else { 
        // Close unused pipe ends
        close(pipe_parent_to_child[1]); // close write end of parent->child pipe
        close(pipe_child_to_parent[0]); // close read end of child->parent pipe
        
        // read message from parent
        read(pipe_parent_to_child[0], buffer, sizeof(buffer));
        printf("%s", buffer); // print message from parent verbatim
        
        // prepare and send message to parent
        snprintf(buffer, sizeof(buffer), "Daddy, my name is %d", getpid());
        write(pipe_child_to_parent[1], buffer, strlen(buffer) + 1);
        
        // close remaining pipe ends
        close(pipe_parent_to_child[0]);
        close(pipe_child_to_parent[1]);
        
        exit(EXIT_SUCCESS);
    }
    
    return 0;
}
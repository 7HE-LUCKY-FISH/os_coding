#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

void print_usage(const char *progName) {
    fprintf(stderr, "Usage: %s -n NumbersToGenerate -r MaxNumber [-p MaxPowerBallNumber] -N NumberSetsToGenerate\n", progName);
}

int main(int argc, char *argv[]) {
    int numbersToGenerate = 0;   // how many numbers to generate per set
    int maxNumber = 0;           // the maximum number in the pool
    int maxPowerBall = 0;        // maximum powerball number (optional)
    int numberSets = 0;          // how many sets to generate
    int i;                      // loop counter


    //settings flag to check for each parameter and prints out a specific error message if one is missing
    bool flag_n = false;
    bool flag_r = false;
    bool flag_N = false;
    // parse command line arguments. And return 1 if there is an error.
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            flag_n = true;
            if (i + 1 < argc)
                numbersToGenerate = atoi(argv[++i]);
            else {
                fprintf(stderr, "Error: Missing value for -n.\n");
                print_usage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "-r") == 0) {
            flag_r = true;
            if (i + 1 < argc)
                maxNumber = atoi(argv[++i]);
            else {
                fprintf(stderr, "Error: Missing value for -r.\n");//gives error code if there is no value for -r
                print_usage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 < argc)
                maxPowerBall = atoi(argv[++i]);
            else {
                fprintf(stderr, "Error: Missing value for -p.\n");//error code if not value for p 
                print_usage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "-N") == 0) {
            flag_N = true;
            if (i + 1 < argc)
                numberSets = atoi(argv[++i]);
            else {
                fprintf(stderr, "Error: Missing value for -N.\n");//error code for value N 
                print_usage(argv[0]);
                return 1;
            }
        } else {
            fprintf(stderr, "Error: Unknown parameter %s\n", argv[i]);// other inputs in the command which aren't covered like -z
            print_usage(argv[0]);
            return 1;
        }
    }
    //checks for values which are needed such as r, n, N
    // check for missing required parameters individually
    if (!flag_n) {
        fprintf(stderr, "Error: Missing required parameter -n.\n");
        print_usage(argv[0]);
        return 1;
    }
    if (!flag_r) {
        fprintf(stderr, "Error: Missing required parameter -r.\n");
        print_usage(argv[0]);
        return 1;
    }
    if (!flag_N) {
        fprintf(stderr, "Error: Missing required parameter -N.\n");
        print_usage(argv[0]);
        return 1;
    }

    // additional validation if values are negative or zero
    if (numbersToGenerate <= 0 || maxNumber <= 0 || numberSets <= 0) {
        fprintf(stderr, "Error: Missing or invalid required parameters such as negative values.\n");
        print_usage(argv[0]);
        return 1;
    }

    if (numbersToGenerate > maxNumber) {
        fprintf(stderr, "Error: NumbersToGenerate (%d) cannot be greater than MaxNumber (%d).\n", numbersToGenerate, maxNumber);
        return 1;
    }

    // seed the random number generator
    srand((unsigned int) time(NULL));

    // generate the lottery sets
    for (int set = 0; set < numberSets; set++) {
        // Allocate an array for the pool of numbers
        int *pool = malloc(maxNumber * sizeof(int));
        if (pool == NULL) {
            fprintf(stderr, "Error: Memory allocation failed.\n");
            return 1;
        }

        // initialize the pool with numbers 1 to maxNumber
        for (i = 0; i < maxNumber; i++) {
            pool[i] = i + 1;
        }

        for (i = maxNumber - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            int temp = pool[i];
            pool[i] = pool[j];
            pool[j] = temp;
        }

        // printing values
        for (i = 0; i < numbersToGenerate; i++) {
            printf("%d", pool[i]);
            if(i != numbersToGenerate - 1){
                printf(",");//make sure the values printed are comma separated until the end
            }
        }
        // if a powerball number is in command line
        if (maxPowerBall > 0) {
            int powerball = (rand() % maxPowerBall) + 1;
            printf(",%d", powerball);
        }
        printf("\n");


        // free the pool no memeory leak
        free(pool);
    }

    return 0;
}

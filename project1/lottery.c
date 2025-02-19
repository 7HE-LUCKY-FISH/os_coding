#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
void print_usage(const char *progName) {
    fprintf(stderr, "Usage: %s -n NumbersToGenerate -r MaxNumber [-p MaxPowerBallNumber] -N NumberSetsToGenerate\n", progName);
}

int main(int argc, char *argv[]) {
    int numbersToGenerate = 0;   // How many numbers to generate per set.
    int maxNumber = 0;           // The maximum number in the pool.
    int maxPowerBall = 0;        // Maximum powerball number (optional).
    int numberSets = 0;          // How many sets (lines) to generate.
    int i;                      // Loop counter.


    //settings flag to check for each parameter and prints out a specific error message if one is missing
    bool flag_n = false;
    bool flag_r = false;
    bool flag_N = false;
    //if there is no N input still generate 1 set
    // Parse command line arguments. And return 1 if there is an error.
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
                fprintf(stderr, "Error: Missing value for -r.\n");
                print_usage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 < argc)
                maxPowerBall = atoi(argv[++i]);
            else {
                fprintf(stderr, "Error: Missing value for -p.\n");
                print_usage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "-N") == 0) {
            flag_N = true;
            if (i + 1 < argc)
                numberSets = atoi(argv[++i]);
            else {
                fprintf(stderr, "Error: Missing value for -N.\n");
                print_usage(argv[0]);
                return 1;
            }
        } else {
            fprintf(stderr, "Error: Unknown parameter %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

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

    // seed the random number generator.
    srand((unsigned int) time(NULL));

    // generate the lottery sets.
    for (int set = 0; set < numberSets; set++) {
        // Allocate an array for the pool of numbers.
        int *pool = malloc(maxNumber * sizeof(int));
        if (pool == NULL) {
            fprintf(stderr, "Error: Memory allocation failed.\n");
            return 1;
        }

        // initialize the pool with numbers 1 to maxNumber.
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
        // if a powerball number is requested, generate and print it.
        if (maxPowerBall > 0) {
            int powerball = (rand() % maxPowerBall) + 1;
            printf(",%d", powerball);
        }
        printf("\n");


        // free the pool.
        free(pool);
    }

    return 0;
}

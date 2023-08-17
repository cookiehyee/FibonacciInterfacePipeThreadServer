#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

//CSC 322 PROJECT 2 INTERFACE PROGRAM - HYUN KIM

//Define TRUE and FALSE used to check if SIGUSR1 is received
#define TRUE 1
#define FALSE 0

//Define String type
#define STRING_LENGTH 128
typedef char String[STRING_LENGTH];

//Function Prototyping 
void HandleSIGUSR1(int SignalToHandle);
void WriteFIBOPIPE(String Pipe);

int main(int argc, char* argv[]) {

    String PipeName; //Pipe name passed from the server program

    //Check if the number of command line arguments is valid
    if (argc != 2) {
        perror("ERROR: Invalid number of command line arguments.\n");
        exit(EXIT_FAILURE);
    }

    strcpy(PipeName, argv[1]);

    //Handle SIGUSR1 and check for error
    if(signal(SIGUSR1,HandleSIGUSR1) == SIG_ERR) {
        perror("ERROR: Failed to handle SIGXCPU.\n");
        exit(EXIT_FAILURE);
    }

    //Disable restart behavior when SIGUSR1 interrupts the system call
    if(siginterrupt(SIGUSR1,1) == -1) {
        //Print error and exit if it fails to disable the restart behavior
        perror("ERROR: Failed to disable restart behavior.\n");
        exit(EXIT_FAILURE);
    }

    //Write and send integers to the server program through FIBOPIPE
    WriteFIBOPIPE(PipeName); 
    return(EXIT_SUCCESS);

}

//Function to handle SIGUSR1
void HandleSIGUSR1(int SignalToHandle) {
    extern int SIGUSR1Received;
    SIGUSR1Received = TRUE; //SIGUSR1 signal has been received
    printf("Received a SIGUSR1, stopping loop\n");
}

//Function to write and send integers to the server though FIBOPIPE
void WriteFIBOPIPE(String Pipe) {

    extern int SIGUSR1Received;
    FILE *FIBOWrite;
    int UserInput;

    //Open FIBOPIPE to write to and check for error
    if((FIBOWrite = fopen(Pipe,"w")) == NULL) {
        perror("ERROR: Failed to open and write to FIBOPIPE.");
        exit(EXIT_FAILURE);
    }

    //SIGUSR1 has not yet been sent out at this point
    SIGUSR1Received = FALSE; 

    do { //Read from user until input is 0

        //Abandon read from user if SIGUSR1 is received
        if(SIGUSR1Received) {
            printf("I: Reading from user abandoned\n");
            UserInput = 0;
        }
        
        //Prompt user to enter nth Fibonacci number to send to the server
        else {
            printf("I: Which Fibonacci number do you want : ");
            scanf("%d", &UserInput);
        }

        //Write and send the integer to the server using FIBOPIPE
        fprintf(FIBOWrite,"%d\n",UserInput);
        fflush(FIBOWrite);

    } while(UserInput != 0);

    //Close FIBOPIPE and check for error
    if(fclose(FIBOWrite) == EOF) {
        perror("ERROR: Failed to close FIBOPIPE.\n");
        exit(EXIT_FAILURE);
    } 

    //Print exit message
    printf("I: Interface is exiting\n");

}

//Declare external variable
int SIGUSR1Received; //Variable used to check if SIGUSR1 is received
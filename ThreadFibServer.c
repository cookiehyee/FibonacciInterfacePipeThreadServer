#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

//CSC 322 PROJECT 2 SERVER PROGRAM - HYUN KIM

//Pipe name to be used for the server and interface programs
#define PIPENAME "FIBOPIPE"

//Define TRUE and FALSE used to check if SIGXCPU is received
#define TRUE 1
#define FALSE 0

//Define String type
#define STRING_LENGTH 128
typedef char String[STRING_LENGTH];

//Function prototyping
void CreatePipe();
void HandleSIGXCPU(int Signal);
void SetCPULimit(int TimeLimit);
void ReadExecuteFIBO();
void ReportCPUUsage();
void *FibResult(void *Num);
int ComputeFib(int Num);
void WaitThread();
void CleanInterface();

int main (int argc, char* argv[]) {

    extern int ChildPID;

    //Check if the number of command line arguments is valid
    if (argc != 2) {
        perror("ERROR: Invalid number of command line arguments.\n");
        exit(EXIT_FAILURE);
    }

    CreatePipe(); //Create pipe called FIBOPIPE
    SetCPULimit(atoi(argv[1])); //Set CPU Limit with command line argument

    //Handle SIGXCPU and check for error
    if(signal(SIGXCPU,HandleSIGXCPU) == SIG_ERR) {
        perror("ERROR: Failed to handle SIGXCPU.\n");
        exit(EXIT_FAILURE);
    }

    //Disable restart behavior when SIGXCPU interrupts the system call
    if(siginterrupt(SIGXCPU,1) == -1) {
        //Print error and exit if it fails to disable the restart behavior
        perror("ERROR: Failed to disable restart behavior.\n");
        exit(EXIT_FAILURE);
    }

    //Create a child process to run the interface program and check for error
    if((ChildPID = fork()) == -1) {
        perror("ERROR: Failed to create child process.\n");
        exit(EXIT_FAILURE);
    }

    //If the child process is successfully created 
    else if(ChildPID == 0) {

        //Run the interface program with the pipe's name as a command line argument
        execlp("./ThreadFibInterface","ThreadFibInterface",PIPENAME,NULL);

        //Check for error in running the interface program 
        perror("ERROR: Failed to run ThreadFibInterface.\n");
        exit(EXIT_FAILURE);

    }

    //Read from FIBOPIPE and create & detach threads to compute Fibonacci numbers
    ReadExecuteFIBO();  

    //After closing FIBOPIPE when the interface program ends:
    WaitThread(); //Wait for threads to terminate
    ReportCPUUsage(); //Report CPU Usage
    CleanInterface(); //Clean interface zombie
    unlink(PIPENAME); //Delete FIBOPIPE

    return(EXIT_SUCCESS);

}

//Function that creates a pipe called FIBOPIPE
void CreatePipe() {

    //Delete FIBOPIPE if it already exists
    unlink(PIPENAME);

    //Create a pipe called FIBOPIPE and check for error
    if(mkfifo(PIPENAME, 0700) == -1) {
        perror("Error");
        exit(EXIT_FAILURE);
    }

}

//Function to handle SIGXPU signal 
void HandleSIGXCPU(int SignalToHandle) {

    extern int ChildPID;
    extern int SIGXCPUReceived;
    SIGXCPUReceived = TRUE; //SIGXCPU signal has been received

    //Ignore further SIGXCPU signals
    printf("Received a SIGXCPU, ignoring any more\n");
    signal(SignalToHandle, SIG_IGN);

    //Send SIGUSR1 signal to the interface program
    printf("Received a SIGXCPU, sending SIGUSR1 to interface\n");
    kill(ChildPID,SIGUSR1);
}

//Function to set CPU time limit
void SetCPULimit(int TimeLimit) {

    struct rlimit CPULimit; //Resource limit of this server program
    printf("S: Setting CPU limit to %ds\n", TimeLimit);

    //Get the old CPU limit for the hard limit field and check for error
    if(getrlimit(RLIMIT_CPU,&CPULimit) == -1) {
        perror("ERROR: Failed to get old CPU limit.\n");
        exit(EXIT_FAILURE);
    }

    //Set the new CPU time limit and check for error
    CPULimit.rlim_cur = TimeLimit;
    if(setrlimit(RLIMIT_CPU, &CPULimit) == -1) {
        perror("ERROR: Failed to set CPU limit.\n");
        exit(EXIT_FAILURE);
    }
}

/*  Function to read integers passed by the Interface through FIBOPIPE
    and create & detach threads to compute the nth Fibonacci number     */
void ReadExecuteFIBO() {

    FILE *FIBORead;
    String PipeInput; //Input passed by the interface through FIBOPIPE 
    int NumToCompute; //Integer to perform Fibonacci computation
    extern int SIGXCPUReceived;
    extern int NumThreads;
    pthread_t FibThread; //Thread to compute Fibonacci numbers

    //SIGXCPU has not yet been sent out at this point
    SIGXCPUReceived = FALSE;

    //Open FIBOPIPE to read from and check for error
    if((FIBORead = fopen(PIPENAME,"r")) == NULL) {
        perror("ERROR: Failed to open and read from FIBOPIPE.\n");
        exit(EXIT_FAILURE);
    }

    do { //Read from pipe until number received from interface is 0 

        //Read integers received from the interface program through FIBOPIPE
        fgets(PipeInput,sizeof(PipeInput),FIBORead);
        NumToCompute = atoi(PipeInput);

        //Do not create and detatch threads if SIGXCPU is received 
        if((NumToCompute) != 0 && !SIGXCPUReceived) {

            ReportCPUUsage(); //Report CPU Usage 
            printf("S: Received %d from interface\n", NumToCompute);

            //Create thread to compute and display the nth Fibonacci number
            if(pthread_create(&FibThread,NULL,FibResult,(void*)&NumToCompute) != 0) {
                perror("ERROR: Failed to create ComputeFib thread.\n");
                exit(EXIT_FAILURE);
            }

            //Detatch the thread and check for error
            if(pthread_detach(FibThread) != 0) {
                perror("ERROR: Failed to detatch ComputeFib thread.\n");
                exit(EXIT_FAILURE);
            }

            //Increment number of threads after sucessful thread creation and detatchment
            NumThreads++; 
            printf("S: Created and detached the thread for %d\n", NumToCompute);

        }

    } while(NumToCompute != 0);

    //Close FIBOPIPE and check for error
    if(fclose(FIBORead) == EOF) {
        perror("ERROR: Failed to close FIBOPIPE.\n");
        exit(EXIT_FAILURE);
    } 

}

//Function to report CPU Usage 
void ReportCPUUsage() {

    struct rusage CPUUsage; //Resource usage of this server program

    //Get and display CPU Usage
    getrusage(RUSAGE_SELF,&CPUUsage); 
    printf("S: Server has used %lds %ldus\n", 
        CPUUsage.ru_utime.tv_sec + CPUUsage.ru_stime.tv_sec,
        CPUUsage.ru_utime.tv_usec + CPUUsage.ru_stime.tv_sec);
}

//Function executed by the thread to compute and display the nth Fibonacci number
void *FibResult(void *Num) {
    
    extern int NumThreads;
    int NumToCompute = *((int *)Num);
    int Fibonacci = ComputeFib(NumToCompute);

    //Display computed nth Fibonacci number 
    printf("S: Fibonacci %d is %d\n", NumToCompute, Fibonacci);

    //Decrement number of threads and terminate thread 
    NumThreads--;
    return NULL; 
}

//Function to compute the nth Fibonacci number using recursion
int ComputeFib(int Num) {
    if(Num <= 1) {
        return Num;
    }
    return (ComputeFib(Num - 1) + ComputeFib(Num - 2));
}

//Function to wait for all threads to terminate
void WaitThread() {

    extern int NumThreads;

    //Wait for all threads to terminate
    while(NumThreads != 0) { 
        printf("S: Waiting for %d threads\n", NumThreads);
        sleep(1); //Sleep to wait for one second 
    }
}

//Function to clean the interface program zombie 
void CleanInterface() {

    extern int ChildPID;
    int ZombieChild; //PID of the dead interface program
    int Status;  //Status of the dead interface program

    //waitpid() used to clean the dead interface program
    ZombieChild = waitpid(ChildPID,&Status,0);

    //Check if the interface program ended abnormally
    if(!WIFEXITED(Status)) {
        perror("ERROR: Interface failed to end normally.\n");
        exit(EXIT_FAILURE);
    }
    
    //Print exit status of the interface program
    printf("S: Child %d completed with status %d", ZombieChild, WEXITSTATUS(Status));
}

//Declare external variables
int ChildPID; //PID of the interface program
int SIGXCPUReceived; //Variable used to check if SIGXCPU is received
int NumThreads; //Number of threads running
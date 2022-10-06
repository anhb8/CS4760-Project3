#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "config.h"
#include <string.h>
#include <sys/shm.h>

char *ofile= "cstest";
FILE *file, *lfile;
int shmid;
struct sharedM *shmp;

char logfile[10]="logfile.";
char logNum[3]; //Process number

struct timeval  now;
struct tm* local;
	
//Deallocate shared memory
void removeSharedMemory() {
        //Detach the process
         if (shmdt(shmp) == -1) {
                perror("Error: shmdt");
                exit(EXIT_FAILURE);
        }

}

//Interrupt signal (^C)
void siginit_handler() {
	removeSharedMemory();
        exit(EXIT_FAILURE);
}

void enterMessage(int n){
	gettimeofday(&now, NULL);
	local = localtime(&now.tv_sec);	

	file=fopen(ofile,"a");
	lfile=fopen(logfile,"a");

	//Write to log file
        fprintf(lfile,"%02d:%02d:%02d Process %d entered the critical section\n",local->tm_hour, local->tm_min, local->tm_sec,n);

	//Write to cstest file
        fprintf(file,"%02d:%02d:%02d File modified by process number %d\n",local->tm_hour, local->tm_min, local->tm_sec,n);
	fclose(lfile);
	fclose(file);
}

void exitMessage(int n){
        gettimeofday(&now, NULL);
        local = localtime(&now.tv_sec);
	
	file=fopen(ofile,"a");
        lfile=fopen(logfile,"a");
        
	//Write to log file
        fprintf(lfile,"%02d:%02d:%02d Process %d exited the critical section\n",local->tm_hour, local->tm_min, local->tm_sec,n);
	
	//Write to cstest file
        fprintf(file,"%02d:%02d:%02d File modified by process number %d\n",local->tm_hour, local->tm_min, local->tm_sec,n);
        fclose(lfile);
	fclose(file);
}

void process (const int i,int num) {
        int j;
	srand(time(0)+i);
	int ran=rand() % 3 + 1; //Generate a random number from 1-3
        char numP[2]; //Process number
       
        signal(SIGALRM, siginit_handler);	
	
	for ( int k = 0; k < 5; k++ ) { 
                do {
                        shmp->flag[i] = want_in;
                        j=shmp->turn;
                        while (j!=i) {
                                if (shmp->flag[j]!=idle) {
                                        j=shmp->turn;
                                } else {
                                        j=(j+1)%num;
                                }   
                        }   
                        //Declare intention to enter critical section
                        shmp->flag[i] = in_cs;

                        //Check that no one else is in critical section
                        for (j=0;j<num;j++){ 
                                if ((j!=i) && (shmp->flag[j]== in_cs)) {
                                        break;
                                }   
                        }   

                } while(j<num || (shmp->turn != i && shmp->flag[shmp->turn]!= idle));
                    
                //Enter critical section
                shmp->turn=i;
	        sleep(ran);	
		enterMessage(i);

		sleep(ran);
                exitMessage(i);

                //Exit critical section
                j= (shmp->turn+1) %num;
                while (shmp->flag[j]==idle) {
                        j= (j+1)%num;

                }
                
		//Assign turn to next waiting process
                shmp->turn =j;
                shmp->flag[i]=idle;   
                
   	}
}


int main(int argc, char *argv[]) {
	//Interrupt signal handler
	signal(SIGINT, siginit_handler);

	int numP=atoi(argv[0]); //Process number
	int nProcess=atoi(argv[1]); //Number of process can run 

	sprintf(logNum,"%d",numP);
        strncat(logfile,logNum,2);	
	//Shared memory ID 
	shmid=shmget(SHM_KEY, sizeof(struct sharedM), 0644);
	if (shmid == -1) {
                perror("Error:shmget");
                exit(EXIT_FAILURE);
        }

	//Attach slave process to shared memory segment
	shmp=(struct sharedM *) shmat(shmid, NULL, 0);
        if (shmp == (struct sharedM *) -1) {
                perror("Error:shmat");
                exit(EXIT_FAILURE);
        }

	
	process(numP,nProcess);
  	removeSharedMemory(); 
	return 0;
}

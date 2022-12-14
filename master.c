#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h> // #define
#include <ctype.h> //isdigit
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h> //shared memory
#include <sys/shm.h> //shared memory
#include <time.h> //local time
#include <sys/time.h>
#include <sys/sem.h>
#include "config.h"

pid_t all_cProcess[MAX_PROCESS];

int shmid;
struct sharedM *shmp;
FILE *file; //Log file

char logfile[10]="logfile.";
char logNum[3]; //Process number
struct timeval  now;
struct tm* local;
int n_process=-1;
int semID;

//Find empty index
int findIndex() {
	int i;
	for (i=0;i<n_process;i++) {
		if (all_cProcess[i]==0)
			return i;
	}
	return -1;
}
//Create semaphore
void createSem() {
	const unsigned short unlocked = 1;
	semID = semget(key, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
	if(semID < 0) {
    		perror("Error: semget failed");
		exit(EXIT_FAILURE);
	}

	if (semctl(semID, 0, SETVAL, unlocked) == -1){
    		perror("Error: semctl failed");
    		exit(EXIT_FAILURE);
  	}
}

//Allocate shared memory
int createSharedMemory() {
	//Create shared memory segment
	shmid=shmget(SHM_KEY, sizeof(struct sharedM), 0644|IPC_CREAT);
	if (shmid == -1) {
      		perror("Error:shmget");
      		exit(EXIT_FAILURE);
   	}
	
	return shmid;
}

//Attach the process to shared memory segment just created - pointer
void *attachSharedMemory() {
	shmp=(struct sharedM *) shmat(shmid, NULL, 0);
	if (shmp == (struct sharedM *) -1) {
      		perror("Error:shmat");
      		exit(EXIT_FAILURE);
   	}	
}

//Deallocate shared memory
void removeSharedMemory() {
	//Detach the process
	 if (shmdt(shmp) == -1) {
      		perror("Error: shmdt");
      		exit(EXIT_FAILURE);
   	}

	//Remove shared memory segment
	if (shmctl(shmid, IPC_RMID, 0) == -1) {
      		perror("Error: shmctl");
     	 	exit(EXIT_FAILURE);
  	}

	//Remove semaphore
	if (semctl(semID, 0, IPC_RMID) == -1) {
                perror("semctl");
                exit(1);
        }
}
void logTermination(pid_t p){ 
        file=fopen("logfile","a");
        gettimeofday(&now, NULL);
        local = localtime(&now.tv_sec);
        fprintf(file,"%02d:%02d:%02d Process %d - Terminated\n",local->tm_hour, local->tm_min, local->tm_sec,p);
        fclose(file);
}

//Interrupt signal (^C) 
void siginit_handler () {
	for (int i=0; i<n_process; i++) {
		if(all_cProcess[i] != 0){ 
                        kill(all_cProcess[i],SIGTERM); 
                        logTermination(all_cProcess[i]);
                }  
	}
	removeSharedMemory();
	exit(EXIT_FAILURE);
}


//Signal when the program runs more than time limit
void alarm_handler () {
	fprintf(stderr,"Error: Exceed time limit\n");
	
	//Kill all processes if exceeds time limit
	for (int i=0; i<n_process; i++) {
		if(all_cProcess[i] != 0){
			kill(all_cProcess[i],SIGTERM); 
			logTermination(all_cProcess[i]);
		}
        } 
        removeSharedMemory();
	exit(EXIT_FAILURE);	
}
int IDtoIndex(pid_t p){
	int i;
	for(i = 0; i<n_process;i++){
		if(all_cProcess[i] == p)
			return i;
	}
	return -1;
}
void forkProcess(int n_process) {
	int i;
	int availSpot = n_process;
	int totalProc = 0;
	pid_t p;

	//for (i=0;i<n_process;i++) {
	while(1){
		pid_t pid = fork();
		if (pid<0) {
			perror("Error: Fork failed");
			removeSharedMemory();
			exit(EXIT_FAILURE);
			
		} else if (pid == 0) {  
			//Child process
			totalProc++;
			char procID[20] = {"\0"};
			char ordNum[10] = {"\0"};
			pid_t p = getpid();

			sprintf(procID,"%d",p);
			sprintf(ordNum,"%d",totalProc);
			
			execl("slave",procID,ordNum,NULL);
			
			//if execl failed
			perror("Error: execl failed ");
			exit(EXIT_FAILURE);
		} else {			//Parent process
			totalProc++;
			availSpot--;;
			int index=findIndex();
			all_cProcess[index] = pid;
			
			if (availSpot == 0) {
				p = wait(NULL);
				logTermination(p);
				int index = IDtoIndex(p);
			       	if(index == -1){
					fprintf(stderr,"Error: Can't find process ID in the list\n");
					siginit_handler();
				}	
				all_cProcess[index] = 0;
				availSpot++;
			} else {
				if((p = waitpid(-1, NULL, WNOHANG)) != 0){
					logTermination(p);
                                	int index = IDtoIndex(p);
                           	     	if(index == -1){
                                        	fprintf(stderr,"Error: Can't find process ID in the list\n");
                                        	siginit_handler();
                               	 	}
                                	all_cProcess[index] = 0;
                                	availSpot++;
				}	
			}

		}
	}

	while((p = wait(NULL)) > 0) {
		logTermination(p);
	}	
}

int validNum(char* sec){
	int size = strlen(sec);
	int i = 0;
	while(i < size){
		if(!isdigit(sec[i]))
			return 0;
		i++;
	}
	return 1;
}

int main(int argc, char *argv[])
{	
	int sec=100;
	int option;
	FILE *file;	
	signal(SIGINT, siginit_handler);
	
	while(optind < argc){
		if ((option = getopt(argc,argv, "ht:")) !=-1) {
			switch(option){
				case 'h':
					printf("%s -h: HELP MENU\n",argv[0]);
                                        printf("Command:\n");
					printf("./master -t ss n	Specify maximum time\n");
					printf("./master n              Default maximum time:100\n");
                                        printf("\nss: Maximum time in seconds after which the process should terminate itself if not completed (Default: 100)\n");
                                        printf("n: The maximum processes that the program runs at a time (From 1-20)\n");
					exit(EXIT_FAILURE);

				case 't':
					if(validNum(optarg)){
						sec = atoi(optarg);
					}else{
						fprintf(stderr,"%s: ERROR: %s is not a valid number\n",argv[0],optarg);
						return EXIT_FAILURE;
					}
					break;
				case '?':
					if(optopt == 't')
						fprintf(stderr,"%s: ERROR: -%c without argument\n",argv[0],optopt);
					else
						fprintf(stderr, "%s: ERROR: Unrecognized option: -%c\n",argv[0],optopt);
					return EXIT_FAILURE;
			}
		} else {
			if(n_process == -1) {
				if(validNum(argv[optind])) {
					n_process = atoi(argv[optind]);	
					
					//Validate if number of processes doesn't exceed 20
					if(n_process >MAX_PROCESS) {
						fprintf(stderr,"%s: Error: Number of processes exceeds 20\n",argv[0]);
                				exit(1);
					}

				} else {
                                       fprintf(stderr,"%s: ERROR: %s is not a valid number\n",argv[0],argv[optind]);
                                       return EXIT_FAILURE;
                                }

			}
			else{
				fprintf(stderr,"Error: There are too many file name\n");
				return EXIT_FAILURE;
			}
			optind++;		
		}
		
	}
	
	signal(SIGALRM, alarm_handler);
        alarm(sec);

	if (n_process ==-1) {
		fprintf(stderr,"%s: Error: Missing number of processes\n",argv[0]);
		exit(1);
	} 
	//Validate if number of processes doesn't exceed 20
        else if (n_process >MAX_PROCESS || n_process <1) {
        	fprintf(stderr,"%s: Error: Number of processes needs to be from 1-20\n",argv[0]);
                exit(EXIT_FAILURE);
        } 
	//MAIN CODE
	createSharedMemory();
        attachSharedMemory(); 
	createSem();	
	
	forkProcess(n_process);
       	
	removeSharedMemory();
	return 0;
}

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
}

//Interrupt signal (^C) 
void siginit_handler () {
	file=fopen("logfile","a");
	printf("-Ctrl C triggered\n");
	for (int i=0; i<n_process; i++) {
                gettimeofday(&now, NULL);
        	local = localtime(&now.tv_sec);
		kill(all_cProcess[i],SIGTERM);
                fprintf(file,"%02d:%02d:%02d Process %d - Terminated\n",local->tm_hour, local->tm_min, local->tm_sec,i);
        
	}
	removeSharedMemory();
	fclose(file);
	exit(EXIT_FAILURE);

}


//Signal when the program runs more than time limit
void alarm_handler () {
	file=fopen("logfile","a");
	fprintf(stderr,"Error: Exceed time limit\n");
	
	//Kill all processes if exceeds time limit
	for (int i=0; i<n_process; i++) {
        	gettimeofday(&now, NULL);
        	local = localtime(&now.tv_sec);
		kill(all_cProcess[i],SIGTERM); 
		fprintf(file,"%02d:%02d:%02d Process %d - Terminated\n",local->tm_hour, local->tm_min, local->tm_sec,i);
        } 
	fclose(file);
        removeSharedMemory();
	exit(EXIT_FAILURE);	
}

void forkProcess(int n_process,int sec) {
	char num[2];
	char nProcess[2];
	int i;
	sprintf(nProcess,"%d",n_process);	
	for (i=0;i<n_process;i++) {
		sprintf(num,"%d",i);
		pid_t pid = fork();
		if (pid<0) {
			perror("Error: Fork failed");
			removeSharedMemory();
			exit(EXIT_FAILURE);
			
		} else if (pid == 0) {  	//Child process
			execl("slave",num,nProcess,NULL);
		} else {			//Parent process
			all_cProcess[i]=pid;

		}
	}
	while(wait(NULL) > 0) {

		//Terminated succesfully
		if (wait(NULL) <0) { 
			file=fopen("logfile","a");
			gettimeofday(&now, NULL);
			local = localtime(&now.tv_sec);
			fprintf(file,"%02d:%02d:%02d Process %d - Terminated\n",local->tm_hour, local->tm_min, local->tm_sec,i);
			fclose(file);
		}
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
	//printf("%d",n_process);
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
 
	
	forkProcess(n_process,sec);
       	removeSharedMemory();
        
	return 0;
}

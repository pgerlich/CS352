/**
 * A basic shell in C using a jobStack struct and a jobStruct to track background jobs. 
 * Functions are well documented and should explain what's going on to a good degree of detail. There are functions for 
 * Handling normal commands and piped commands. All variable names are generally descriptive.
 * By: Paul Gerlich
 */

//CLib imports
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

/*
 * Object declarations
 
 * These objects are a stack of jobs (to track running and finished jobs) and the job item that sits in the stack
 */ 
 
 typedef struct job job;
 
//Object representing a single job
struct job {
	//Internal id for tracking
	int id;

	//The string representing the command
	char command[500];
	
	//system process id
	int pId;
	
	//For management in the stack LL structure
	job* next;
	job* prev;
};

//job stack object for tracking current jobs
typedef struct {
	//Head node of running jobs
	job* running;

	//Head node of finished jobs
	job* finished;

	//Index of next job (starts @ 1)
	int jobIndex;

	//Number of running jobs (if == 0, reset jobIndex to 1) 
	int runningCount;
	
	//Number of finished jobs
	int finishedJobs;
} jobStack;

/*
 * End object declarations
 */
 
//Function declarations
void doMainTasks();

void addJob(char* command, int pid);
void updateJobs();
void printJobStack();

void waitForProcess(char* command);
void waitForBackgroundTasks();
void changeWorkingDirectory(char* command);
int executePipeCommands(char* command);
int executeCommand(char* command);
int executePipeCommands(char* command);
int executeNormalCommand(char* command, int isBackgroundTask, int redirectInput, int redirectOutput);

char** convertCommandToArray(char* command);
char* getInputLocation(char* commandArray);
char* getOutputLocation(char* commandArray);

//Single instance of the jobStack
jobStack jobs;

int main(int argc, char** argv) {
	int running = 1;

	//MAIN LOOP
	doMainTasks();

	return 1;
}

//Main loop functions
void doMainTasks(){
	//Initialize jobStack
	jobs.jobIndex = 1;
	jobs.runningCount = 0;
	jobs.finishedJobs = 0;

	//Continue executing until the end of input
	while ( !feof(stdin) ) {
		//Wait for commands
		printf("wdh: ");
		fflush(stdout);	

		//Read the command
		char in[256];
		fgets(in, 256, stdin);
		in[strlen(in) - 1] = '\0';
		
		//printf("COMMAND WAS: %s\n", in);
		//printf("COMMAND LENGTH: %d\n", strlen(in));

		//Check for custom commands
		if ( strcmp(in, "exit") == 0 ) {
			waitForBackgroundTasks();
			return;
		} else if ( strstr(in, "cd") ) {
			changeWorkingDirectory(in);
		} else if ( strstr(in, "wait") ) {
			waitForProcess(in);
		} 
		
		//Execute normal commands
		else {
			//Returns null for foreground and invalid jobs, returns process id upon succesfully starting a background job
			int pid = executeCommand(in);
			
			//Break for child thread (returns -1 for child thread)
			if ( pid < 0 ){
				break;
			}
		}
		
		//Clear finished jobs and replace them with new finished jobs
		updateJobs();
		printJobStack();	
	}

}

/*
 * Start JobStack management
 */

//create the job for tracking
void addJob(char* command, int pid){
	//Create our job -- Malloc so it's on the heap not stack
	job* currentJob = (job*) malloc(sizeof(job));
	
	currentJob->id = jobs.jobIndex;
	
	strcpy(currentJob->command, command);
	currentJob->pId = pid;
	
	//Increment job counter
	jobs.jobIndex++;
	jobs.runningCount++;

	//Place job in LL
	currentJob->next = jobs.running;
	
	if ( jobs.running ) {
		jobs.running->prev = currentJob;
	}
	
	jobs.running = currentJob;
}

//Clear the old finished jobs, and add any new ones
void updateJobs(){
	//Clear the finished jobs
	job* cur = jobs.finished;
	job* next = cur;
	
	int i = 0;
	for(i = 0; i < jobs.finishedJobs; i++ ) {
		next = cur->next;
		free(cur);
		cur = next;
	}

	jobs.finishedJobs = 0;
	
	//Set the new finished jobs to nada
	cur = jobs.running;
		
	int jobsRemoved = 0;
	
	for(i = 0; i< jobs.runningCount; i++ ) {
		//Check status
		int status;
		waitpid(cur->pId, &status, WNOHANG);
		
		//printf("Status [%d] is {%d}\n",i+1,status);
		
		//Move finished jobs to finished LL
		if ( !status ) {
			if ( cur->next ) {
				cur->next->prev = cur->prev;		
			}
			
			if ( cur->prev ) {
				cur->prev->next = cur->next;	
			}
			
			if ( jobs.finished ) {
				cur->next = jobs.finished;
				jobs.finished->prev = cur;
			}
			
			jobs.finished = cur;
			jobs.finishedJobs++;
			
			jobsRemoved++;
		}
		
		cur = cur->next;
	}

	//Update the # of jobs running and reset the index if necessary
	jobs.runningCount -= jobsRemoved;
	if ( jobs.runningCount == 0 ) {
		jobs.jobIndex = 1;
	}
}

//Function for printing out the current jobs in the stack
void printJobStack(){
	job* cur = jobs.running;
	int i = 0;
	
	//Iterate over running jobs
	printf("Running:\n");
	
	for(i = 0; i < jobs.runningCount; i++ ) {
		printf("[%d] %s \n", cur->id, cur->command);
		cur = cur->next;
	}
	
	//Iterate over finished jobs
	printf("Finished:\n");
	
	cur = jobs.finished;
	
	for(i = 0; i < jobs.finishedJobs; i++ ) {
		printf("[%d] %s \n", cur->id, cur->command);
		cur = cur->next;
	}

}
/*
 * End job stack management
 */
 
 /*
 * Command execution
 */
 void waitForProcess(char* command){
	char** commandArray = convertCommandToArray(command);
		
	int id = atoi(commandArray[1]);
	
	//parameter was not a valid number
	if ( !id ) {
		printf("Not a valid ID. Wait command syntax was incorrect.\n");
		return;
	}
	
	//Search jobs for this specified id
	int i;
	int found = 0;
	job* cur = jobs.running;
	for(i = 0; i < jobs.runningCount; i++ ) {
		
		//Wait if we find the process
		if ( cur->id == id ) {
			printf("Waiting for [%d]\n",id);
			found = 1;
			waitpid(cur->pId, NULL, 0);
			break;
		}
	}
	
	if ( !found ) {
		printf("[%d] was not found.\n", id);
	}
	
	return;
 }
 
 //Before honoring exit request, complete background jobs
 void waitForBackgroundTasks(){
	//First thing - Update the jobs that are done so there's less overhead here.
	updateJobs();
	
	int i, status;
	job* cur = jobs.running;
	
	//Now wait for each job to complete (in the opposite order in which they were received). Order doesn't matter, just wait.
	for(i = 0; i < jobs.runningCount; i++ ) {
		waitpid(cur->pId, &status, 0);
	}
	
 }

//Changes the working directory
void changeWorkingDirectory(char* command){
	char** commandArray = convertCommandToArray(command);
	
	int success = chdir(commandArray[1]);
	
	//Graceful failure or ls on success
	if ( success < 0 ) {
		printf("Failed to change directory.\n");
		return;
	} else {
		printf("Starting ls\n");
		char* cmd = "ls";
		executeNormalCommand(cmd, 0, 0, 0);
	}
}
 
//Executes the command in either the foreground or background
int executeCommand(char* command){
	//Determine if this is a background task or not
	int isBackgroundTask = 0;
	int redirectInput = 0;
	int redirectOutput = 0;
	
	//Check for background task, update command
	if ( command[strlen(command) - 1] == '&' ) {
		isBackgroundTask = 1;
		command[strlen(command) - 1] = '\0';
	}
	
	//Determine if we have pipes and execute accordingly
	if ( strchr(command, '|') != NULL )  {
		
		//Don't handle background pipes
		if ( isBackgroundTask ) {
			printf("Background pipes are not supported.\n");
			return 0;
		}
		
		//Don't handle pipes and redirects
		if ( strchr(command, '<') != NULL || strchr(command, '>') != NULL ) {
			printf("The use of pipes and redirects is not supported.\n");
			return 0;
		}
		
		return executePipeCommands(command);
	} 
	
	//redirected input
	if ( strchr(command, '<') != NULL ) {
		redirectInput = 1;
	}
	
	//redirected output
	if ( strchr(command, '>')  != NULL  ) {
		redirectOutput = 1;
	}
	
	//Handle all other commands
	return executeNormalCommand(command, isBackgroundTask, redirectInput, redirectOutput);
}

//Executes a normal command without pipes
int executeNormalCommand(char* command, int isBackgroundTask, int redirectInput, int redirectOutput){
	char* cmdCpy = (char*) malloc(sizeof(char) * strlen(command));
	strcpy(cmdCpy, command);
	char** commandArray = convertCommandToArray(command);
	
	pid_t pid;
	int status;
	pid = fork();
	
	//Child
	if (pid == 0) {
		FILE* input;
		FILE* output;
		
		//Setup file for redirecting input
		if ( redirectInput ) {
			input = fopen(getInputLocation(cmdCpy), "r");
			if ( input ) {
				dup2(fileno(input), STDIN_FILENO);
				fclose(input);
			} else {
				printf("Opening file for STDIN redirection failed.\n");
				return -1;
			}
		}
		
		//Setup file for redirecting output
		if ( redirectOutput ) {
			output = fopen(getOutputLocation(cmdCpy), "w");
			if ( output ) {
				dup2(fileno(output), STDOUT_FILENO);
				fclose(output);
			} else {
				printf("Opening file for STDOUT redirection failed.\n");
				return -1;
			}
		}
		
		execvp(commandArray[0], commandArray);
		return -1;
	} 
	
	//Parent (us)
	else {
		//We block for foreground, don't block for background
	    if (!isBackgroundTask) {
		   waitpid(pid, &status, 0);
		   if ( status ) {
			   printf("Something went wrong. Perhaps your command was invalid.\n");
		   }
		   return 0;
	    } else {
			addJob(cmdCpy, pid);
			return pid;
		}
	}
}

//Execute piped commands
int executePipeCommands(char* command){
	char* cmdCpy = (char*) malloc(sizeof(char) * strlen(command));
	strcpy(cmdCpy, command);
	
	int pipeCount = 0;
	
	char* cur = strtok(command, "|");
	
	//Count the pipes
	while ( cur ) {
		pipeCount++;
		cur = strtok(NULL, "|");
	}
	
	//printf("Number of pipe commands [%d]\n", pipeCount);
	
	//Make an array for each command in the pipeline
	char** pipeArray = (char**) malloc(sizeof(char*) * (pipeCount + 1));
	
	//Copy our array back over
	strcpy(command, cmdCpy);
	
	//Now add each pipe command to the array of commands
	cur = strtok(command, "|");
	int pipeArrayIndex = 0;
	
	//Copy the pipes
	while ( cur ) {
		pipeArray[pipeArrayIndex] = (char*) malloc(sizeof(char) * strlen(cur));
		strcpy(pipeArray[pipeArrayIndex], cur);
		//printf("Read pipe cmd: {%s}\n", cur);
		cur = strtok(NULL, "|");
		pipeArrayIndex++;
	}
	
	//Setup pipe variables for tracking
	int fd[2];
	pid_t pid;
	int fd_in = STDIN_FILENO;
	
	int i;
	for(i = 0; i < pipeCount; i++ ) {
		//Convert the command to what's needed
		char** commandArray = convertCommandToArray(pipeArray[i]);
		
		pipe(fd);
		
		//Failure
		if ( (pid = fork()) == -1 ) {
				perror("fork");
				exit(1);
		} else if( pid == 0 ) {
			dup2(fd_in, STDIN_FILENO);
			
			//Passing along 
			if ( i != (pipeCount - 1) ) {
				dup2(fd[1], STDOUT_FILENO);
			}
			
			//Close in
			close(fd[0]);
				
			execvp(commandArray[0], commandArray);
			
			//Cancel out of child process
			exit(0);
			
		} else {
			//Wait for prev to finish
			wait(NULL);
			
			//Close out
			close(fd[1]);
			
			//Save the input
			fd_in = fd[0];
			
			//printf("Will now read from: %d\n", fd_in);
		}
	}
	
	return 0;
}
 
 /*
 * End command execution
 */
 
 /*
  * Misc functions
  */ 
 
//Given a command of the form 'cmd arg1 arg2 ...' it returns the array {cmd, arg1, arg2, ...}
char** convertCommandToArray(char* command){
	
	//Count the spaces -- there are N+2 arguments (including the null pointer)
	int i;
	int count = 0;
	for( i = 0; i < strlen(command); i++ ) {
		char c = command[i];
		if ( c == ' ' ) {
			count++;
		}
	}
	
	count+= 2;
		
	//Now create our result array and fill it with these damn arguments
	char** resultArray = (char**) malloc(sizeof(char*) * count);
	
	int resultIndex = 0;
	
	//If the command has any spaces (i.e, more than one argument)
	if ( count > 2 ) {
		char* arg = strtok(command, " ");
		
		while ( arg ) { 
			//Skipping redirection stuff
			if ( strchr(arg, '<') != NULL || strchr(arg, '>') != NULL) {
				//Skip this element (the Redirect character)
				arg = strtok(NULL, " ");
				
				//Skip next element) (the redirect file)
				arg = strtok(NULL, " ");
			} else {
				//Copy as usual
				resultArray[resultIndex] = (char*) malloc(sizeof(char) * strlen(arg));
				strcpy(resultArray[resultIndex], arg);
				resultIndex++;
				
				arg = strtok(NULL, " ");
				
			}
		}
	} else {
		resultArray[0] = (char*) malloc(sizeof(char) * strlen(command));
		strcpy(resultArray[0], command);
		resultIndex++;
	}
	
	//for( i = 0; i <= resultIndex; i++ ) {
	//	printf("%d -- %s\n", i, resultArray[i]);
	//}
	
	return resultArray;
}

//Gets the input file for a redirect
char* getInputLocation(char* command){
	//Make our copy
	char* cpy = (char*) malloc(sizeof(char) * strlen(command));
	strcpy(cpy, command);

	char* arg = strtok(cpy, " ");
	
	//Go through and find the input file
	while ( arg ) {
		if( strchr(arg, '<') != NULL ) {
			arg = strtok(NULL, " ");
			break;
		}
		arg = strtok(NULL, " ");
	}
	
	return arg;
}

//Gets the output file for a redirect
char* getOutputLocation(char* command){
	//Make our copy
	char* cpy = (char*) malloc(sizeof(char) * strlen(command));
	strcpy(cpy, command);
	
	char* arg = strtok(cpy, " ");
	
	//Go through and find the input file
	while ( arg ) {
		if( strchr(arg, '>') != NULL ) {
			arg = strtok(NULL, " ");
			break;
		}
		arg = strtok(NULL, " ");
	}
	
	free(cpy);
	
	return arg;
}

 /*
  * End Misc functions
  */ 

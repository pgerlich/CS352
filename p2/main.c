/**
	A rudimentary multi-threaded encryption program. I used a custom queue implementation to manage the buffer. This queue has node objects representing an object in the queue/buffer to be written or moved.
	
	Functions:
	readInput: Continously read input from a file placing things in the input buffer
	countInput: Continously count things in the input buffer
	encryptInput: Continously encrypt items in the input buffer, remove them from the buffer, and push them in the output buffer
	countOutput: Continously count things in the output buffer
	write: Continously write things to the output file from the output buffer
	

*/

//CLib imports
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

/*
 * Object declarations
 
 * These objects are a stack of jobs (to track running and finished jobs) and the job item that sits in the stack
 */ 
 
typedef struct node node;
 
//Object representing a single characted item in a buffer/queue
struct node {
	char c; //This character

	int counted; //If it has been counted yet or not
	int encrypted; //If it has been encrypted yet or not
	
	node* prev; //One directional LL as Queue
};

//buffer queue
typedef struct {
	node* head; //Front of q (for dequeueing)
	node* tail; // Back of q (for enqueuing)

	int capacity; //Max size
	int count; //current size
} queue;

/*
 * End object declarations
 */

//Prototypes
void* readInput(void* args);
void* countInput(void* args);
void* encryptInput(void* args);
void* countOutput(void* args);
void* writeOutput(void* args);
void debug(char* msg);

//Shared data
//Count variables
int inputCount[255];
int outputCount[255];
int bufSize;

//I/O buffers
queue input_bufferq;
queue output_bufferq;

//Semaphores for mutual exclusion
sem_t read_in;
sem_t count_in;
sem_t encrypt_in;
sem_t encrypt_out;
sem_t count_out;
sem_t write_out;

//I/O Files
FILE * inFile;
FILE * outFile;

//debugging for output
int debugging = 0;

int main(int argc, char** argv) {
	pthread_t in, icount, en, ocount, out;
	
	//Validate argument size
	if ( argc != 3 ) {
		printf("Incorrect format. Should be: ./encrypt inputfile outputfile \n");
		exit(0);
	}
	
	//Try to open files
	inFile = fopen(argv[1], "r");
	outFile = fopen(argv[2], "w");
	
	if ( inFile == NULL ) {
		printf("Input file doesn't exist \n");
		exit(0);
	}
	
	//Read Input
	printf("Enter Buffer Size:");
	fflush(stdout);	

	char bufSizeReader[256];
	fgets(bufSizeReader, 256, stdin);
	bufSizeReader[strlen(bufSizeReader) - 1] = '\0';

	//Convert it to an int
	bufSize = atoi(bufSizeReader);
	
	//Initialize shared variables
	//Buffers
	input_bufferq.capacity = bufSize;
	input_bufferq.count = 0;
	output_bufferq.capacity = bufSize;
	output_bufferq.count = 0;
	
	//Initialize semaphores
	sem_init(&read_in, 0, 1);
	sem_init(&count_in, 0, 0);
	sem_init(&encrypt_in, 0, 0);
	sem_init(&encrypt_out, 0, 1);
	sem_init(&count_out, 0, 0);	
	sem_init(&write_out, 0, 0);
	
	//Create threads
	//readInput(NULL);
	pthread_create(&in, NULL, readInput, NULL);
	pthread_create(&icount, NULL, countInput, NULL);
	pthread_create(&en, NULL, encryptInput, NULL);
	pthread_create(&ocount, NULL, countOutput, NULL);
	pthread_create(&out, NULL, writeOutput, NULL);
	
	//Wait for completion
	pthread_join(in, NULL);
	pthread_join(icount, NULL);
	pthread_join(en, NULL);
	pthread_join(ocount, NULL);
	pthread_join(out, NULL);
	
	printf("Input Counts: \n");

	int i;
	for(i = 0; i < 255; i++ ) {
		if ( inputCount[i] > 0 && ((char) i) != '\n' ) {
			printf("%c %d \n",(char) i, inputCount[i]);
		}
	}

	printf("Output Counts: \n");
	
	for(i = 0; i < 255; i++ ) {
		if ( outputCount[i] > 0  && ((char) i) != '\n' ) {
			printf("%c %d \n",(char) i, outputCount[i]);
		}
	}
	
	return 1;
}

//Enqueueing objects into a buffer queue
//TODO Never: Memory management doe
int enqueue(queue* q, char c){
	if ( q->count == q->capacity ) {
		return 0;
	} 
	
	node* newChar = (node*) malloc(sizeof(node));
	newChar->counted = 0;
	newChar->encrypted = 0;
	newChar->c = c;
	
	if ( q->count == 0) {
		q->head = newChar;
		q->tail = newChar;
	} else {
		q->tail->prev = newChar;
		q->tail = newChar;
	}
	
	q->count = q->count + 1;
	
	return 1;
}

//Dequeue an object from the buffer
//TODO: Memory management
node* dequeue(queue* q){
	if ( q->count == 0 ) {
		return (node*)NULL;
	}
	
	//Grab node, move head pointer (could end up being null)
	node* curNode = (node*) malloc(sizeof(node));
	curNode->c = q->head->c;
	curNode->counted = q->head->counted;
	curNode->encrypted = q->head->encrypted;
	
	q->head = q->head->prev;
	
	//Set tail to null if this was our only element
	if ( q->count == 1 ) {
		q->tail = NULL;
	}
	
	q->count--;
	
	return curNode;
}

/**
	Encrypts characters with this methodology:
		
	1) s = 1;
	2) Get next character c.
	3) if c is not a letter then goto (7).
	4) if (s==1) then increase c with wraparound (e.g., 'A' becomes 'B', 'c' becomes 'd', 'Z' becomes 'A', 'z' becomes 'a'), set s=-1, and goto (7).
	5) if (s==-1) then decrease c with wraparound (e.g., 'B' becomes 'A', 'd' becomes 'c', 'A' becomes 'Z', 'a' becomes 'z'), set s=0, and goto (7).
	6) if (s==0), then do not change c, and set s=1.
	7) Encrypted character is c.

*/
char encrypt(char c, int* s){
	int cVal = (int) c;
	
	if ( (cVal >= 65 && cVal <= 90) || (cVal >= 97 && cVal <= 122) ) {
		//C is a letter
		switch (*s) {
			
			//Decrease w/ wraparound
			case -1:
				*s = 0;
				if (cVal == 65 ) {
					cVal = 90;
				} else if (cVal == 97 ) {
					cVal = 122;
				} else {
					cVal = cVal - 1;
				}
				return (char) cVal;
			
			//Leave
			case 0:
				*s = 1;
				return c;
				
			//Increase w/ wraparound
			case 1:
				*s = -1;
				if ( cVal == 90 ) {
					cVal = 65;
				} else if ( cVal == 122 ) {
					cVal = 97;
				} else {
					cVal = cVal + 1;
				}
				return (char) cVal;
		}
	} else {
		return c;
	}
}

/** 
	Encrypts input of the input buffer, passing them to the output buffer one at a time
	
	Waits on: Input & output buffers to be available to touch (signaled by count in or writer)
	Signals: Read in and count out
*/
void* encryptInput(void* args){
	node* curIn;
	node* temp;
	int s = 1;
	int wasProcessed = 1;
	
	while ( 1 ) {
		
		//Wait on input buffer
		sem_wait(&encrypt_in);

		debug("in encryption\n");
		
		curIn = input_bufferq.head;
	
		//Encrypt as many as we want
		while ( curIn != NULL ) {
			//debug("encryption not null\n");
			if ( curIn->counted && !curIn->encrypted) {
				if ( curIn->c != EOF && curIn->c != '\n' ) {
					curIn->c = encrypt(curIn->c, &s); //Encrypt char, adjust S value	
				}
				
				curIn->encrypted = 1;
				debug("encrypted something\n");
				
				break;
			}
			
			curIn = curIn->prev;
		}
		
		//Establish if the sequential next element is ready to process
		if (input_bufferq.count > 0 && input_bufferq.head->encrypted){
			temp = dequeue(&input_bufferq);
			//debug("Ready to go to output\n");
			//Signal input buffer
			sem_post(&read_in);
		}

		//Wait on output buffer
		sem_wait(&encrypt_out);
		
		//If we can process this element
		enqueue(&output_bufferq, temp->c);
		debug("Pushed to output\n");
		
		sem_post(&count_out);

		if ( temp->c == EOF ) {
			debug("--------FINISHED ENCRYPTING\n");
			break;
		}
	}
}

/**
	Continously counts the character occurences in the output buffer
	
	Waits on: Output Buffer to be available to touch (signaled by encryption)
	Signals: Writer (to file)
*/
void* countOutput(void* args){
	int i;
	node* cur;
	
	while ( 1 ) {
		//Wait on output
		sem_wait(&count_out);
		
		cur = output_bufferq.head;
		
		debug("in output\n");
		while ( cur != NULL ) {
			//debug("In output not null\n");
			if ( !cur->counted ) {
				outputCount[cur->c] = outputCount[cur->c] + 1; //Increment this character count
				cur->counted = 1;
				
				sem_post(&write_out);
				
				debug("Counted some output \n");
				
				if ( cur->c == EOF ) {
					debug("--------FINISHED COUNTING OUT\n");
					return (void*) NULL;
				} else {
					break;
				}
			} else {
				cur = cur->prev;
			}
		}
	}
}

/**
	Continously counts the character occurences in the input buffer
	
	Waits on: Input Buffer to be available to touch (signaled by readinput)
	Signals: Encryption
*/
void* countInput(void* args){
	int i;
	node* cur;

	while ( 1 ) {
		//Wait on input
		sem_wait(&count_in);
		
		cur = input_bufferq.head;
		
		debug("In counting\n");
		while ( cur != NULL ) {
			//debug("in count not null\n");
			if ( cur->counted == 0 ) {
				inputCount[cur->c] = inputCount[cur->c] + 1; //Increment this character count
				cur->counted = 1;
				
				sem_post(&encrypt_in);
				
				debug("Counted some input \n");
				
				if ( cur->c == EOF ) {
					debug("--------FINISHED COUNTING IN\n");
					return (void*) NULL;
				} else {
					break;					
				}
			} else {
				cur = cur->prev;
			}	
		}

	}
	
}

/**
	Continously read input from a file byte-by-byte, writing to a buffer.
	
	Waits on: Input Buffer to be available to touch (signaled by encryption)
	Signals: Counting thread
*/
void* readInput(void* args){
	char cur;
	
	cur = fgetc(inFile);
	
	while ( 1 ) {
		//WAIT on input
		sem_wait(&read_in);
		
		if ( enqueue(&input_bufferq, cur) ) {
			debug("Placed char in buffer (in)\n");
			
			sem_post(&count_in);
			
			if ( cur == EOF) {
				debug("--------FINISHED READING\n");
				break;
			} else {
				cur = fgetc(inFile);
			}
		}
		
	}
}

/**
	Write to the output file continously.
	
	Waits on: Output Buffer to be available to touch (signaled by output counter)
	Signals: Encryption
*/
void* writeOutput(void* args){
	node* cur;
	
	while ( 1 ) {
		//WAIT on output
		sem_wait(&write_out);
		
		cur = output_bufferq.head;

		//Iterate over each node, but break as soon as we find one that isn't counted to maintain order
		if ( cur->counted ) {
			dequeue(&output_bufferq);
			
			if ( cur->c == EOF ) {
				break;
			}
			
			fputc(cur->c, outFile);
			fflush(outFile);
			
			cur = cur->prev;
		} 
		
		sem_post(&encrypt_out);
	}
	
	debug("-----------Finishing writing output\n");
}

/**
	Function for outputting debug messages when applicable
*/
void debug(char* msg){
	if ( debugging ) {
		printf("%s", msg);
	}
}
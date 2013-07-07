//Compile and run with: gcc -O3 -o work_crew work_crew.c queue_utils.c -std=c99 -lpthread -fopenmp && reset && ./work_crew kandasamy /home/DREXEL/nk78 8
//Solution number is 1273 times

// John Carto and Julian Kemmerer
// ECEC 622 Assignment 1
// 4/26/13

#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "queue.h"

//Lazy code including
#include "omp_crew.c"

// Function prototypes
void search_for_string_serial(char **);
void search_for_string_pthreads(char **);
void *pthread_func(void *args);

//Global variables for pthreads
int num_occurences;
int numThreads;
char g_searchString[MAX_LENGTH];
queue_t *queue;
sem_t queue_work; // counting sem -> # of items in queue
sem_t queue_lock ; // binary sem -> protect queue
sem_t wait_lock;  
sem_t count_lock;
int threadsWaiting;
int searchDone;
sem_t wait_lock;


int main(int argc, char** argv)
{
		  if(argc < 3){
			 printf("Usage: %s <search string> <path> <# of threads> \n", argv[0]);
			 exit(0);
		  }

		  search_for_string_openmp(argv);
		  search_for_string_pthreads(argv);
		  search_for_string_serial(argv);
		  
		  exit(0);
}

/* Given a search string, the function performs a single-threaded or serial search of the file system starting from the specified path name. */
void search_for_string_serial(char **argv)
{
	  printf("SERIAL Execution Started.\n");
		  
	  int num_occurences = 0;
	  queue_element_t *element, *new_element;
	  struct stat file_stats;
	  int status; 
	  DIR *directory = NULL;
	  struct dirent *result = NULL;
	  struct dirent *entry = (struct dirent *)malloc(sizeof(struct dirent) + MAX_LENGTH); // Allocate memory for the directory structure

	  /* Create and initialize the queue data structure. */
	  queue_t *queue = create_queue();
	  element = (queue_element_t *)malloc(sizeof(queue_element_t));
	  if(element == NULL){
				 printf("Error allocating memory. Exiting. \n");
				 exit(-1);
	  }
	  strcpy(element->path_name, argv[2]);
	  element->next = NULL;
	  insert_in_queue(queue, element); // Insert the initial path name into the queue

	  /* Start the timer here. */
	  struct timeval start;	
	  gettimeofday(&start, NULL);

	  /* While there is work in the queue, process it. */
	  while(queue->head != NULL){
				 queue_element_t *element = remove_from_queue(queue);
				 status = lstat(element->path_name, &file_stats); // Obtain information about the file pointed to by path_name
				 if(status == -1){
							printf("Error obtaining stats for %s \n", element->path_name);
							free((void *)element);
							continue;
				 }
				 if(S_ISLNK(file_stats.st_mode)){ // Check if the file is a symbolic link; if so ignore it
				 } 
				 else if(S_ISDIR(file_stats.st_mode)){ // Check if file is a directory; if so descend into it and post work to the queue
							//printf("%s is a directory. \n", element->path_name);
							directory = opendir(element->path_name);
							if(directory == NULL){
									  //printf("Unable to open directory %s \n", element->path_name);
									  continue;
							}
							while(1){
									  status = readdir_r(directory, entry, &result); // Store the directory item in the entry data structure; if result == NULL, we have reached the end of the directory
									  if(status != 0){
												 //printf("Unable to read directory %s \n", element->path_name);
												 break;
									  }
									  if(result == NULL)
												 break; // End of directory
									  /* Ignore the "." and ".." entries. */
									  if(strcmp(entry->d_name, ".") == 0)
												 continue;
									  if(strcmp(entry->d_name, "..") == 0)
												 continue;

									  /* Insert this directory entry in the queue. */
									  new_element = (queue_element_t *)malloc(sizeof(queue_element_t));
									  if(new_element == NULL){
												 printf("Error allocating memory. Exiting. \n");
												 exit(-1);
									  }
									  /* Construct the full path name for the directory item stored in entry. */
									  strcpy(new_element->path_name, element->path_name);
									  strcat(new_element->path_name, "/");
									  strcat(new_element->path_name, entry->d_name);
									  insert_in_queue(queue, new_element);
							}
							closedir(directory);
				 } 
				 else if(S_ISREG(file_stats.st_mode)){ // Check if file is a regular file
							//printf("%s is a regular file. \n", element->path_name);
							FILE *file_to_search;
							char buffer[MAX_LENGTH];
							char *bufptr, *searchptr;
							
							/* Search the file for the search string provided as the command-line argument. */ 
							file_to_search = fopen(element->path_name, "r");
							if(file_to_search == NULL){
									  //printf("Unable to open file %s \n", element->path_name);
									  continue;
							} 
							else{
									  while(1){
												 bufptr = fgets(buffer, sizeof(buffer), file_to_search);
												 if(bufptr == NULL){
															if(feof(file_to_search)) break;
															if(ferror(file_to_search)){
																	  printf("Error reading file %s \n", element->path_name);
																	  break;
															}
												 }
												 searchptr = strstr(buffer, argv[1]);
												 if(searchptr != NULL){
															//printf("Found string %s within file %s. \n", argv[1], element->path_name);
															num_occurences ++;
															// getchar();
												 }
									  }
							}
							fclose(file_to_search);
				 }
				 else{
							//printf("%s is of type other. \n", element->path_name);
				 }
				 free((void *)element);
	  }

	  /* Stop timer here and determine the elapsed time. */
	  struct timeval stop;	
	  gettimeofday(&stop, NULL);
	  printf("SERIAL Overall execution time = %fs. \n", (float)(stop.tv_sec - start.tv_sec + (stop.tv_usec - start.tv_usec)/(float)1000000));
	  printf("SERIAL The string %s was found %d times within the file system. \n\n", argv[1], num_occurences);
}

void search_for_string_pthreads(char **argv){
	printf("PTHREAD Execution Started.\n");
	
	/* set numThreads and string to search */
	numThreads = atoi(argv[3]);
	strcpy(g_searchString,argv[1]);
	threadsWaiting = 0;
	searchDone = 0;
	
	/* declare pthread array and initialize semaphores */
	pthread_t threads[numThreads];
	if ( sem_init(&queue_work,0,0) < 0 ){
		perror("sem_init error");
		exit(0);
	}
	if ( sem_init(&queue_lock,0,1) < 0 ){
		perror("sem_init error");
		exit(0);
	}
	if ( sem_init(&wait_lock,0,1) < 0 ){
		perror("sem_init error");
		exit(0);
	}
	if ( sem_init(&count_lock,0,1) < 0 ){
		perror("sem_init error");
		exit(0);
	}

	/* Create and initialize the queue data structure. */
	queue = create_queue();
	queue_element_t *firstElement;
	firstElement = (queue_element_t *)malloc(sizeof(queue_element_t));
	if(firstElement == NULL){
		printf("Error allocating memory. Exiting. \n");
		exit(-1);
	}
	strcpy(firstElement->path_name, argv[2]);
	firstElement->next = NULL;
	insert_in_queue(queue, firstElement); // Insert the initial path name into the queue
	sem_post(&queue_work);   // b/c adding first element

	/* Start the timer here. */
	struct timeval start;	
	gettimeofday(&start, NULL);

	/* Spawn threads*/
	int i;
	for (i = 0; i < numThreads; i++) {
		pthread_create(&threads[i],NULL,pthread_func,(void *) i);
	}

	/* wait for threads to join */
	for (i = 0; i < numThreads; i++) {
		pthread_join(threads[i],NULL);
	}

	/* Stop timer here and determine the elapsed time. */
	struct timeval stop;	
	gettimeofday(&stop, NULL);
	printf("PTHREAD Overall execution time = %fs. \n", (float)(stop.tv_sec - start.tv_sec + (stop.tv_usec - start.tv_usec)/(float)1000000));
	printf("PTHREAD The string %s was found %d times within the file system. \n\n", argv[1], num_occurences);
}

/* worker pthread function */
void *pthread_func(void *args) {

	/* threadID*/
	int tID = (int)args;
	
	int stringCount = 0;
	queue_element_t *element, *new_element;
	struct stat file_stats;
	int status; 
	DIR *directory = NULL;
	struct dirent *result = NULL;
	struct dirent *entry = (struct dirent *)malloc(sizeof(struct dirent) + MAX_LENGTH); // Allocate memory for the directory structure
	char temp[MAX_LENGTH];

	while(1) {

		/* increase threadWaiting count, set DONE flag if all threads waiting*/
		sem_wait(&wait_lock);
		threadsWaiting++;
		if (threadsWaiting == numThreads){
			searchDone = 1;

			/* add count to global count */
			sem_wait(&count_lock);
			num_occurences = num_occurences + stringCount;
			sem_post(&count_lock);
			
			/* wake up other threads */
			int i;
			for(i = 0; i < numThreads; i++){
				sem_post(&queue_work);
			}
			printf("Thread #%d finished.\n", tID);
			pthread_exit(0);
		}
		sem_post(&wait_lock);


		/* wait for work in queue */
		sem_wait(&queue_work);

		
		if (searchDone) {
			
			/* add count to global count */
			sem_wait(&count_lock);
			num_occurences = num_occurences + stringCount;
			sem_post(&count_lock);
			
			printf("Thread #%d finished.\n", tID);
			pthread_exit(0);
		}

		/* decrease thread waiting count */
		sem_wait(&wait_lock);
		threadsWaiting--;
		sem_post(&wait_lock);

		/* acquire queue lock and retrieve directory from queue */
		sem_wait(&queue_lock);
		element = remove_from_queue(queue);
		sem_post(&queue_lock);

		/* Open directory */
		directory = opendir(element->path_name);
		if(directory == NULL){
			//printf("Unable to open directory %s \n", element->path_name);
			free( (void *) element); //added
			continue;
		}

		/* Go through directory file-by-file */
		while(1){
			
			/* store file in entry  --> if result == NULL, end of directory */
			status = readdir_r(directory, entry, &result); 
			if(status != 0){
					 printf("Unable to read directory %s \n", element->path_name);
					 break;
			}
			if(result == NULL)
					 break; // End of directory

			/* Ignore the "." and ".." entries. */
			if(strcmp(entry->d_name, ".") == 0)
					 continue;
			if(strcmp(entry->d_name, "..") == 0)
					 continue;
			
			/* Construct full path name */
			strcpy(temp, element->path_name);
			strcat(temp, "/");
			strcat(temp, entry->d_name);

			/* Get information about file */
			status = lstat(temp, &file_stats); // Obtain information about the file pointed to by path_name
			if(status == -1){
				printf("Error obtaining stats for %s \n", temp);
				continue;
	 		}

			/* if symlink, ignore */
			if(S_ISLNK(file_stats.st_mode)){ 
			
			/* if directory, add back to queue */
			}else if(S_ISDIR(file_stats.st_mode)){
				//printf("%s is a directory. \n", element->path_name);

				/* Malloc space for new queue element */
				new_element = (queue_element_t *)malloc(sizeof(queue_element_t));
				if(new_element == NULL){
						 printf("Error allocating memory. Exiting. \n");
						 exit(-1);
				}

				/* Construct full path name */
				strcpy(new_element->path_name, element->path_name);
				strcat(new_element->path_name, "/");
				strcat(new_element->path_name, entry->d_name);

				/* Acquire queue lock, add to queue, signal work sem */
				sem_wait(&queue_lock);
				insert_in_queue(queue, new_element);
				sem_post(&queue_work);
				sem_post(&queue_lock);
			
			/* if file, count # of occurences */
			}else if(S_ISREG(file_stats.st_mode)){ 
				//printf("%s is a regular file. \n", temp);

				FILE *file_to_search;
				char buffer[MAX_LENGTH];
				char *bufptr, *searchptr;

				/* Search the file for the search string provided as the command-line argument. */ 
				file_to_search = fopen(temp, "r");
				if(file_to_search == NULL){
						  //printf("Unable to open file %s \n", temp);
						  continue;
				} 
				else{
				  	while(1){
						 bufptr = fgets(buffer, sizeof(buffer), file_to_search);
						 if(bufptr == NULL){
							if(feof(file_to_search)) break;
							if(ferror(file_to_search)){
								printf("Error reading file %s \n", temp);
								break;
							}
						 }

						 searchptr = strstr(buffer, g_searchString);
						 if(searchptr != NULL){
							//printf("Found string %s within file %s. \n", g_searchString, temp);
							stringCount++;
							// getchar();
						 }
					}
				}
				fclose(file_to_search);

			/* if file is of some other type */
			} else{
							printf("%s is of type other. \n", element->path_name);
			}
	 		
		}
		closedir(directory);
		free( (void *) element);
	}	
}

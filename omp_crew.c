#include <omp.h>
#include <sched.h>

//Queue helper
unsigned int queue_empty(queue_t * queue)
{
	if(queue->head == NULL){ // Queue is empty
		return 1;
	}
	return 0;
}
unsigned int queue_size(queue_t * queue)
{
	unsigned int size = 0;
	queue_element_t *current = queue->head;
	while(current != NULL){
		size++;
		current = current->next;
	}
}

//Given a queue try to pull some items off
//Let's abuse the things we were given
//Use the queue as a list, order doesn't matter. Why rewrite code;
//Return queue of strings, and the number of items in there
queue_t * get_items(queue_t * queue, unsigned int num_items, unsigned int * num_returned)
{
	//Allocate a queue
	queue_t * ret_queue = create_queue();
	
	//Remove elements until reaching null or the number requested
	*num_returned = 0;
	#pragma omp critical
	{
		while(1)
		{
			//Try to remove an item
			queue_element_t * item = remove_from_queue(queue);
			if(item == NULL)
			{
				break;
			}
			
			//Add this item to the queue
			insert_in_queue(ret_queue, item);
			*num_returned = *num_returned +1;
			
			if(*num_returned >=num_items)
			{
				break;
			}
		}
	}
	
	
	//printf("Got %u/%u from queue\n", *num_returned, num_items);
	
	return ret_queue;
}

void add_item_to_correct_queue(queue_element_t * element, queue_t * glb_file_queue, queue_t * glb_dir_queue)
{
	int status;
	//Find out if this is a file or a directory
	struct stat file_stats;
	// Obtain information about the file pointed to by path_name
	status = lstat(element->path_name, &file_stats); 
	if(status == -1){
			printf("Error obtaining stats for %s \n", element->path_name);
			free((void *)element);
			return;
	}
	// Check if the file is a symbolic link; if so ignore it
	if(S_ISLNK(file_stats.st_mode))
	{
	}
	//Is a dir?
	else if(S_ISDIR(file_stats.st_mode))
	{
		//Add this element to dir queue
		insert_in_queue(glb_dir_queue,element);
	}
	//Reg file?
	else if(S_ISREG(file_stats.st_mode))
	{ 
		insert_in_queue(glb_file_queue,element);
	}
	else
	{
		printf("Unsure what to do with: %s \n", element->path_name);
	}
}

void process_dir_struct(DIR *directory, queue_t * glb_file_queue, queue_t * glb_dir_queue, char * dir)
{	
	int status;
	struct dirent *result = NULL;
	// Allocate memory for the directory structure
	struct dirent *entry = (struct dirent *)malloc(sizeof(struct dirent) + MAX_LENGTH);
	
	#pragma omp critical
	{
	while(1)
	{
		// Store the directory item in the entry data structure; 
		//if result == NULL, we have reached the end of the directory
		status = readdir_r(directory, entry, &result); 
		if(status != 0){
				 printf("Unable to read directory %s \n", dir);
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
		queue_element_t * new_element = (queue_element_t *)malloc(sizeof(queue_element_t));
		if(new_element == NULL){
				 printf("Error allocating memory. Exiting. \n");
				 exit(-1);
		}
		/* Construct the full path name for the directory item stored in entry. */
		strcpy(new_element->path_name, dir);
		strcat(new_element->path_name, "/");
		strcat(new_element->path_name, entry->d_name);
		add_item_to_correct_queue(new_element, glb_file_queue, glb_dir_queue);
	}
}
	closedir(directory);
}

void process_dir_w_stat(char * dir, queue_t * glb_file_queue, queue_t * glb_dir_queue, struct stat * file_stats)
{
	//Directory struct
	DIR *directory = NULL; 
	
	// Check if the file is a symbolic link; if so ignore it
	if(S_ISLNK(file_stats->st_mode))
	{ 
	} 
	// Check if file is a directory 
	// if so descend into it and post work to the queue
	else if(S_ISDIR(file_stats->st_mode))
	{
		//printf("%s is a directory. \n", dir);
		directory = opendir(dir);
		if(directory == NULL)
		{
				  //printf("Unable to open directory %s \n", dir);
				  return;
		}
		
		//All good to process dir struct
		process_dir_struct(directory, glb_file_queue, glb_dir_queue, dir);
	} 
}

//Lookat this dir and add files/dirs to global queues
void process_dir(char * dir, queue_t * glb_file_queue, queue_t * glb_dir_queue)
{
	//This code will likely be similar to the code given originally
	int status;
	struct stat file_stats;
	
	// Obtain information about dir
	status = lstat(dir, &file_stats); 
	if(status == -1)
	{
			printf("Error obtaining stats for %s \n", dir);
	}
	
	//Process w stat
	process_dir_w_stat(dir, glb_file_queue, glb_dir_queue, &file_stats);
}

//Look at this file and fine the to_find string
void process_file(char * filename, char * to_find, unsigned int * thread_specific_num_found)
{
	//printf("%s is a regular file. \n", filename);
	FILE *file_to_search;
	char buffer[MAX_LENGTH];
	char *bufptr, *searchptr;
	
	/* Search the file for the search string provided as the command-line argument. */ 
	file_to_search = fopen(filename, "r");
	if(file_to_search == NULL){
			  printf("Unable to open file %s \n", filename);
			  return;
	} 
	else{
			  while(1){
						 bufptr = fgets(buffer, sizeof(buffer), file_to_search);
						 if(bufptr == NULL){
									if(feof(file_to_search)) break;
									if(ferror(file_to_search)){
											  printf("Error reading file %s \n", filename);
											  break;
									}
						 }
						 searchptr = strstr(buffer, to_find);
						 if(searchptr != NULL){
									//printf("Found string %s within file %s. \n", to_find, filename);
									(*thread_specific_num_found)++;
									// getchar();
						 }
			  }
	}
	fclose(file_to_search);
}

//Given dirs just for this thread and global file queue
void process_dirs(queue_t * loc_dir_queue, queue_t * glb_file_queue, queue_t * glb_dir_queue)
{
	//We have a list of dirs
	//Loop through each of them
	while(1)
	{
		queue_element_t * item = remove_from_queue(loc_dir_queue);
		if(item == NULL)
		{
			return;
		}
		
		//Get the str from this item
		char * str = item->path_name;
		//Process this dir
		process_dir(str,glb_file_queue, glb_dir_queue);
	}	
}
//Given files just for this thread and global num found
void process_files(queue_t * loc_file_queue, unsigned int * num_found, char * to_find)
{
	unsigned int local_num_found = 0;
	
	//We have a list of files
	//Loop through each of them
	while(1)
	{
		queue_element_t * item = remove_from_queue(loc_file_queue);
		if(item == NULL)
		{
			//Done increment global with local
			(*num_found) +=local_num_found;
			return;
		}
		
		//Get the str from this item
		char * str = item->path_name;
		//Process this dir
		process_file(str, to_find, &local_num_found);
	}	
}

//Check on a queue and do something specific...
//If qtype == 0, directories, 
//qtype ==1, files
void queue_checker(queue_t * queue, 
unsigned int num_items, 
unsigned int qtype, 
queue_t * other_queue, 
unsigned int * num_found, 
char * to_find)
{
	//Alright, let's check if there something in here
	if(queue_empty(queue))
	{
		//Empty, done here.
		return;
	}
	else
	{
		//Queue not empty
		//Try to pull off some items
		
		unsigned int items_got = 0;
		queue_t * tmp_items = get_items(queue, num_items, &items_got);
		
		//Did we get some items?
		if(items_got > 0)
		{
			//Do the specfic task for these items, with specific args
			if(qtype == 0)
			{
				//Dirs
				//printf("%d got dir items\n", items_got);
				process_dirs(tmp_items, other_queue, queue);
			}
			else
			{
				//Files
				//printf("%d got file items\n", items_got);
				process_files(tmp_items, num_found, to_find);
			}
		}
		else
		{
			//No items gotten, done here
			return;
		}
	}
}

//Another layer of help for putting strings on the queue
void place_in_queue(queue_t *queue, char * str)
{
	  queue_element_t * element = (queue_element_t *)malloc(sizeof(queue_element_t));
	  if(element == NULL){
				 printf("Error allocating memory. Exiting. \n");
				 exit(-1);
	  }
	  strcpy(element->path_name, str);
	  element->next = NULL;
	  insert_in_queue(queue, element);
}

//Added last minute...
//Let everyone know if the file_queue hit hit is minimum size before
//starting to look at file - avoids lock step between putting in out
//taking out of queue
unsigned int file_queue_hit_desired = 0;
//Do the searching
void omp_grepr(
char * to_find, 
unsigned int num_threads, 
unsigned int * num_found, 
queue_t * dir_queue, 
queue_t * file_queue,
unsigned int num_files_per_queue_access,
unsigned int num_dirs_per_queue_access,
unsigned int file_queue_min_size)
{
	char * to_find_cpy = malloc(strlen(to_find)+1);
	to_find_cpy = strcpy(to_find_cpy,to_find);
	
	int times_empty = 0;
	while(times_empty<=10000)
	{
		while( (!queue_empty(dir_queue) || !queue_empty(file_queue)) )
		{
			//More directories to search? Also pass global file queue
			queue_checker(dir_queue, num_dirs_per_queue_access, 0, file_queue, num_found, to_find);
			
			//Only check the file queue if a minimum number of files exist within
			//Or if have hit that value before and are on our way down
			if( (file_queue_hit_desired==1) || (file_queue->size >= file_queue_min_size))
			{
				//printf("file queue size %u\n",file_queue->size);
				file_queue_hit_desired = 1;
				//More files to search? Also pass global dir queue
				queue_checker(file_queue, num_files_per_queue_access, 1, dir_queue, num_found, to_find);
			}
			
			//Wait a bit
			sched_yield();
		}
		times_empty++;
		//Wait a bit
		sched_yield();
	}
}

//OpenMP: Given a search string, the function performs a multi-threaded search of the file system starting from the specified path name.
void search_for_string_openmp(char **argv)
{
	//What's going on, eh?
	printf("OPENMP Execution Started.\n");
	
	//  	1			2			3
	//<search string> <path> <# of threads>"
	//Collect the command line args
	char * to_find = argv[1];
	char * start_path = argv[2];
	unsigned int num_threads = atoi(argv[3]);
	unsigned int num_found = 0;
	
	//If there was some way to know where files were located on disk
	//To further optimize disk access patterns... that would be neat.
	
	//Ok two queues, one for directories to be searched through
	//The other is for filenames themselves
	queue_t * dir_queue = create_queue();
	queue_t * file_queue = create_queue();
	
	//We don't want queues being locked
	//unlocked very often so each thread will pull off a certain number
	//of items at a time to look at
	unsigned int num_files_per_queue_access = 10;
	unsigned int num_dirs_per_queue_access = 1;
	
	//Need to build up a buffer of files in the list before starting
	unsigned int file_queue_min_size = 1000;
	
	//So threads will first look to the dir queue to keep finding files
	//If no dirs exist then they will look in the file queue for files
	//to search through
	//Prime the dir queue with the starting directory
	place_in_queue(dir_queue, start_path);	  	
	
	//Start the timer
	struct timeval start;	
	gettimeofday(&start, NULL);
	
	//Let's get parallel up in here
	#pragma omp parallel num_threads(num_threads) shared(dir_queue, file_queue, num_found) private(num_threads)//num_files_per_queue_access, num_dirs_per_queue_access
	{
		//Do the search, yo.
		omp_grepr(to_find, num_threads, &num_found, dir_queue, file_queue,num_files_per_queue_access,num_dirs_per_queue_access,file_queue_min_size);
	}
	
	//Stop timer here and determine the elapsed time
	struct timeval stop;	
	gettimeofday(&stop, NULL);
	printf("OPENMP Overall execution time = %fs. \n", (float)(stop.tv_sec - start.tv_sec + (stop.tv_usec - start.tv_usec)/(float)1000000));
	printf("OPENMP The string %s was found %d times within the file system. \n\n", to_find , num_found);
}

/**
 * This program execute a pipeline with commands. Either from a file given as input
 * to the program or the user types the commands in. The commands need to be presented line by line.
 * Only one command per line. The program is only allowed to have max 2 arguments as indata
 * (the name of the program + one additional arguemnt). If only one parameter (the name of the program) the user
 * will have to type the commands in self. NOTE stop by pressing ctrl + d. For example you can
 * run the program with writing "./mexec" then press enter. After that you can type the commands, for
 * example: seq 1 100 (NOTE it is one command and its arguemnt per line)
 * 		tail -n 3 (NOTE it is one command and its arguemnt per line)
 * 	
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define SIZE 1024
#define READ_END 0
#define WRITE_END 1

//Function declarations.
void quit_if_argc_is_more_than_two(int argc);
void allocate_memory(char ***the_commands, int number_of_commands);
void ensure_memory_allocation(void *ptr);
void opens_file(FILE **file, char *name_of_file);
void load_commands_from_file(char ***the_commands, int *number_of_commands, int *max_commands, FILE **fp);
void load_commands_from_stdin(char ***the_commands, int *number_of_commands, int *max_commands);
void reallocate_twice_as_much_memory(char ***the_commands, int *max_commands);
void remove_new_line_char_from_the_commands(char ***the_commands, int number_of_commands);
void create_pipes(int (*pipes_and_fd)[2], int number_of_pipes);
pid_t create_a_process(void);
void close_all_filedescriptors(int (*pipes_and_fd)[2], int number_of_pipes);
void deallocate_all_memory(char ***the_commands, int *max_commands);
void print_commands(char ***the_commands, int number_of_commands);

//Added
void safe_wait_children(int number_of_commands, int pids[number_of_commands]);
void child_process(int i, int number_of_commands, int (*pipes_and_fd)[2], char ***the_commands);
void safe_dup2_read(int i, int (*pipes_and_fd)[2]);
void safe_dup2_write(int i, int (*pipes_and_fd)[2]);


int main(int argc, char **argv)
{
	FILE *file_pointer;
	int current_max_number_of_commands = 10;
	int number_of_commands = 0;
	char **array_of_commands;

	quit_if_argc_is_more_than_two(argc);
	allocate_memory(&array_of_commands, current_max_number_of_commands);

	if(argc == 2){
		opens_file(&file_pointer, argv[1]);
		load_commands_from_file(&array_of_commands, &number_of_commands, &current_max_number_of_commands, &file_pointer);
	}

	if(argc < 2){
		load_commands_from_stdin(&array_of_commands, &number_of_commands, &current_max_number_of_commands);
	}

	remove_new_line_char_from_the_commands(&array_of_commands, number_of_commands);

	int pipe_and_fildescriptors[number_of_commands - 1][2];
	create_pipes(pipe_and_fildescriptors, number_of_commands - 1);

	int pids[number_of_commands];
	//Creating processes.
	for(int i = 0; i < number_of_commands; i++){
		pid_t id = create_a_process();

		//We are in the child process.
		if(id == 0){
			child_process(i, number_of_commands, pipe_and_fildescriptors, &array_of_commands);
		}
		pids[i] = id;
	}
	close_all_filedescriptors(pipe_and_fildescriptors, number_of_commands - 1);

	//Waits for the child process to finnish and exits if it was terminated incorecctly.
	safe_wait_children(number_of_commands, pids);
	
	//Close file if it was oponed.
	if(argc == 2){
		fclose(file_pointer);
	}

    deallocate_all_memory(&array_of_commands, &current_max_number_of_commands);
    return 0;
}


/**
 * @brief Checks if memory has sucessfully been allocated.
 * 
 * This function checks if memory has been alloated correctly by checking
 * if a pointer is equal to NULL or not. If the pointer is NULL it means that the memory
 * has not been allocated correctly. If that is the case we exit.
 * 
 * @param ptr The pointer to check if it is equal to NULL or not.
 * @return exit(EXIT_FAILURE) if the pointer is NULL, else no returns nothing.
*/
void ensure_memory_allocation(void *ptr)
{
	if (ptr == NULL){
		fprintf(stderr, "Failed to allocate memory\n");
		exit(EXIT_FAILURE);
	}
}


/**
 * @brief Allocates twice as much memory.
 * 
 * This function allocates twice as much memory. Will be used to make sure
 * there is enough space to store all the commands. NOTE that this function
 * uses the "ensure_memory_allocation" that will exit if the memory allocation fails.
 *  
 * @param the_commands A pointer to the array holding the commands.
 * @param max_commands A pointer to the integer representing the maximum number of commands.
 * @return exit_failure if the allocation fails, else returns nothing.
 */
void reallocate_twice_as_much_memory(char ***the_commands, int *max_commands)
{
	int previous_number_of_commands = *max_commands;
	*max_commands = *max_commands * 2;

	*the_commands = realloc(*the_commands, *max_commands * sizeof(char*));
	ensure_memory_allocation(*the_commands);

	for(int i = previous_number_of_commands; i < *max_commands; i++){
		(*the_commands)[i] = malloc(SIZE);
		ensure_memory_allocation((*the_commands)[i]);
	}
}


/**
 * @brief de-allocates all memory for the array containing the commands holds.
 * 
 * This function de-allocates all the memory created for holding the commands.
 * 
 * @param the_commands A pointer to the array that holds the commands.
 * @param max_commands A pointer that points to the maximum number of commands, which is the space allocated.
 * @return Nothing.
 */
void deallocate_all_memory(char *** the_commands, int *max_commands)
{
	for(int i = 0; i < *max_commands; i++){
		free((*the_commands)[i]);
	}
	free(*the_commands);
}


/**
 * @brief This function allocates memory for the array that will hold the commands.
 * 
 * It is the responsibillity of the user to call the deallocate_all_memory to make sure
 * there is no memory leaks. NOTE that this function uses the "ensure_memory_allocation"
 * that will exit if the memory allocation fails.
 * 
 * @param the_commands Pointer to the array containing the commands.
 * @param number_of_commands The number of commands you want to allocate memory for.
 * @return exit(EXIT_FAILURE) if memory allocation fails, else returns nothing.
 */ 
void allocate_memory(char ***the_commands, int number_of_commands)
{
	*the_commands = calloc(number_of_commands, sizeof(char*));
	ensure_memory_allocation(*the_commands);

	for(int i = 0; i < number_of_commands; i++){
		(*the_commands)[i] = malloc(SIZE);
		ensure_memory_allocation(*the_commands);
	}
}


/**
 * @brief Prints out the command at each line.
 * 
 * This function is only used to see if the commands has been loaded to the array correctly.
 * 
 * @param the_commands Pointer to the array containing the commands.
 * @param number_of_commands The number of commands in the array of commands.
 * @return Nothing.
 */ 
void print_commands(char ***the_commands, int number_of_commands)
{
	printf("\n");
	for(int i = 1; i <= number_of_commands; i++){
		printf("Commando at line %d is: %s\n", i, (*the_commands)[i - 1]);
	}
	printf("\n");
}


/**
 * @brief This function removes the newline char ('\n') from the array of commands.
 * @param the_commands A pointer to the array containing the commands.
 * @param number_of_commands The number of commands inside the array.
 * @return Nothing.
 */ 
void remove_new_line_char_from_the_commands(char ***the_commands, int number_of_commands)
{
	for(int i = 0; i < number_of_commands; i++){
		for(size_t j = 0; j < strlen((*the_commands)[i]); j++){
			if((*the_commands)[i][j] == '\n'){
				(*the_commands)[i][j] = '\0';
			}
		}
	}
}


/**
 * @brief This function opens a file with a given name to the function.
 * @param file A pointer to the file to open.
 * @param name_of_file The name of the file
 * @return exit(EXIT_FAILURE) if the file could not be opened, else returns nothing.
 */ 
void opens_file(FILE **file, char *name_of_file)
{
	*file = fopen(name_of_file, "r");
	if(*file == NULL){
		perror("Failed to open the file");
		exit(EXIT_FAILURE);
	}	
}


/**
 * @brief This function quits if the argumnets to the program is more than 2.
 * @param argc The argument count.(Note that the name of the program counts as one argument)
 * @return exit(EXIT_FAILURE) if argc is more than 2. (The name of the program + 1 argument + 1 more argument.)
 * Else retuns nothing.
 */
void quit_if_argc_is_more_than_two(int argc)
{
    if (argc > 2) {
        fprintf(stderr, "To many arguments to the program. Two arguemnts is max\n");
        exit(EXIT_FAILURE);
    }
}


/**
 *@brief This function creates a child process with fork
 *
 * Creates a child process that will be used to execute commands
 * from a pipeline.
 * 
 *@return the child id. Or exit(EXIT_FAILURE) if fork failed.
 */
pid_t create_a_process(void)
{
	pid_t id = fork();
	if(id == -1){
		perror("Fork failed\n");
		exit(EXIT_FAILURE);
	}
	return id;
}


/**
 * @brief This function creates pipes and assignes its filedescriptors.
 * 
 * This function creates pipes that has a READEND and a WRITEEND that allows
 * communication between different processes.
 * 
 * @param pipes_and_fd A pointer to the 2-array containing the pipes and its filedescriptors.
 * @param number_of_pipes The number of pipes you want to create.
 * @return exit(EXIT_FAILURE) if pipe fails, else returns nothing.
 */ 
void create_pipes(int (*pipes_and_fd)[2], int number_of_pipes)
{
	for(int i = 0; i < number_of_pipes; i++){
		if(pipe(pipes_and_fd[i]) == -1){
			perror("Pipe failed\n");
			exit(EXIT_FAILURE);
		}
	}
}


/**
 * @brief This function closes all the filedescriptors connected to the pipes.
 * @param pipes_and_fd A pointer to the 2-array containing the pipes and its filedescriptors.
 * @param number_of_pipes The number of pipes.
 * @return Nothing.
 */ 
void close_all_filedescriptors(int (*pipes_and_fd)[2], int number_of_pipes)
{
	for(int i = 0 ; i < number_of_pipes; i++){
		close(pipes_and_fd[i][READ_END]);
		close(pipes_and_fd[i][WRITE_END]);
	}
}


/**
 * @brief This function loads the commands from stdin into an array holding the commands.
 * 
 * Loads the array with the commands coming coming from stdin, either from a file if the
 * program was called like this: ""./mexec < textfile". Or from the keyboard, the user will then have to type
 * the commands in. The user stops typing commands by pressing ctrl +d. One command on each line. If the number
 * of commands gets more than the maximum number of commands the reallocate_twice_as_much_memory function will
 * be called to make sure there is enough space to store all the commands.
 * 
 * @param the_commands Pointer to the array that will hold the commands.
 * @param number_of_commands Pointer to the number of commands. (This variable will be incremented to the actual
 * number of commands).
 * @param max_commands Pointer to the maximum number of commands.
 * @return Nothing.
 */ 
void load_commands_from_stdin(char ***the_commands, int *number_of_commands, int *max_commands)
{
	char local_buffer[SIZE];
	while(fgets(local_buffer, SIZE, stdin) != NULL){
		if(*number_of_commands == *max_commands){
			reallocate_twice_as_much_memory(the_commands, max_commands);
		}	
		//Copies the command to the array of commands.
		strncpy((*the_commands)[*number_of_commands], local_buffer, SIZE);
		(*number_of_commands)++;
	}
}


/**
 * @brief This function loads the commands from a file into an array holding the commands.
 * 
 * Loads the array with the commands coming fom the gven file. One commands should be represented on
 * each line in the file. If the number of commands gets more than the maximum number
 * of commands the reallocate_twice_as_much_memory function will be called to make sure there is enough
 * space to store all the commands.
 * 
 * @param the_commands Pointer to the array that will hold the commands.
 * @param number_of_commands Pointer to the number of commands. (This variable will be incremented to the actual
 * number of commands).
 * @param max_commands Pointer to the maximum number of commands.
 * @param fp Pointer to the file to load the commands from.
 * @return Nothing.
 */ 
void load_commands_from_file(char ***the_commands, int *number_of_commands, int *max_commands, FILE **fp)
{
	char local_buffer[SIZE];
	while(fgets(local_buffer, SIZE, *fp) != NULL){
		if(*number_of_commands == *max_commands){
			reallocate_twice_as_much_memory(the_commands, max_commands);
		}	
		//Copies the command to the array of commands.
		strncpy((*the_commands)[*number_of_commands], local_buffer, SIZE);
		(*number_of_commands)++;
	}
}


/**
 * @brief This function waits for all children to execute and checks if they terminated correctly.
 * 
 * @param number_of_commands The number of commands
 * @param pids An array of the childrens processing IDs.
 * @return exit(EXIT_FAILURE) if any child process did not terminate correctly, else returns nothing.
*/
void safe_wait_children(int number_of_commands, int pids[number_of_commands])
{
	for(int i = 0; i < number_of_commands; i++){
		int status;
		if(waitpid(pids[i], &status, 0) == -1){
			perror("Child process did not terminate sucessfully");
			exit(EXIT_FAILURE);
		}
		if (WIFEXITED(status) && WEXITSTATUS(status)){
			fprintf(stderr, "Child processs failed to terminate sucessfully\n");
			exit(EXIT_FAILURE);
		}
	}
}



/**
 * @brief This function redirects the filedescriptors, closes them, and executes the command.
 * 
 * This function represents the entire child process. It redirects the filedescriptors so that
 * we always read from the previous pipe as long as we are not in the first process. And we always
 * write to the current pipe as long as we are not in the last process. We close all the
 * filedescriptors in the child process after redirection is completed. Then we execute the command.
 * 
 * @param i Variable to know which iteration we are in. We redirect differently depending on this.
 * @param number_of_commands The number of commands.
 * @param pipes_and_fd A pointer to the 2-array containing the pipes and its filedescriptors.
 * @param the_commands Pointer to the array that holds the commands.
 * @return exit(EXIT_FAILURE) if redirection goes wrong or execution of the command goes wrong. 
 * Else it returns nothing.
*/
void child_process(int i, int number_of_commands, int (*pipes_and_fd)[2], char ***the_commands)
{
	//Always read from previous pipe as long as we are not in the first process.
	if(i != 0){
		safe_dup2_read(i, pipes_and_fd);
	}

	//Always write to the current process as long as we are not in the last process.
	if(i != number_of_commands -1){
		safe_dup2_write(i, pipes_and_fd);
	}

	close_all_filedescriptors(pipes_and_fd, number_of_commands - 1);
	//Execute the command.
	if(execl("/bin/sh","/bin/sh", "-c", (*the_commands)[i], NULL) == -1){
		perror("No such file or directory");
		exit(EXIT_FAILURE);
	}
}


/**
 * @brief This function redirects so that we read from the pipe rather than STDIN
 * @param i Variable to know which iteration we are in.
 * @param pipes_and_fd A pointer to the 2-array containing the pipes and its filedescriptors.
 * @return exit(EXIT_FAILURE) if redirection (dup2) failed, else returns nothing.
*/
void safe_dup2_read(int i, int (*pipes_and_fd)[2])
{
	if(dup2(pipes_and_fd[i - 1][READ_END], STDIN_FILENO) == -1){
		perror("Failed to duplicate file descriptor.");
		exit(EXIT_FAILURE);
	}
}


/**
 * @brief This function redirects so that we write to the pipe rather than STDOUT
 * @param i Variable to know which iteration we are in.
 * @param pipes_and_fd A pointer to the 2-array containing the pipes and its filedescriptors.
 * @return exit(EXIT_FAILURE) if redirection (dup2) failed, else returns nothing.
*/
void safe_dup2_write(int i, int (*pipes_and_fd)[2])
{
	if(dup2(pipes_and_fd[i][WRITE_END], STDOUT_FILENO) == -1){
		perror("Failed to duplicate file descriptor.");
		exit(EXIT_FAILURE);
	}
}


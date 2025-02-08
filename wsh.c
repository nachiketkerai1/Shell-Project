#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include "wsh.h"


// GLOBAL VARS
struct localVarNode *shellVarHead = NULL;
struct History *hist = NULL;
char *wholeCommand;

int returnCode = 0; // returnCode global variable to track what code to exit with
// File Descriptor global vars to update throghout the program where to print the output
int stdoutNo = STDOUT_FILENO;
int stdinNo = STDIN_FILENO;


/* 
Handles redirection for the Linux commands
*/
void redirect_fd(char *redirect, char *filename, int *in, int *out, int *err){

	// Don't do anything if there isnt a redirect
	if (redirect == NULL){
		return;
	}

	if (strcmp(redirect,  ">>") != 0 && strcmp(redirect,  ">") != 0 && strcmp(redirect,  "<") != 0 && strcmp(redirect,  "&>") != 0 && strcmp(redirect,  "&>>") != 0){
		perror("Incorrect redirection syntax");
		return;
	}

	
	if (strcmp(redirect, ">>") == 0){
		*out = open(filename, O_APPEND | O_WRONLY | O_CREAT, 0644);	
		dup2(*out, stdoutNo);	 		
		close(*out);
	} else if (strcmp(redirect, ">") == 0){
		*out = open(filename, O_WRONLY | O_CREAT, 0644);
		dup2(*out, stdoutNo);
		close(*out);	 		
	} else if (strcmp(redirect, "&>") == 0){
		*out = open(filename, O_WRONLY | O_CREAT, 0644);
		*err = *out;
		dup2(*out, STDOUT_FILENO);
		dup2(*err, STDERR_FILENO);
		close(*out);	
	} else if (strcmp(redirect, "&>>") == 0){
		*out = open(filename, O_APPEND | O_WRONLY| O_CREAT, 0644);
		*err = *out;
		dup2(*out, STDOUT_FILENO);
		dup2(*err, STDERR_FILENO);
		close(*out);
	} else if (strcmp(redirect, "<") == 0){
		*in = open(filename, O_RDONLY, 0644);
		dup2(*in, stdinNo);
		close(*in);
	}
}

/* 
Handles redirection for built-in commands
*/
void redirections(char *redirect, char *filename, FILE **in, FILE **out, FILE **err){

	// Don't do anything if there isnt a redirect
	if (redirect == NULL){
		return;
	}

	if (strcmp(redirect,  ">>") != 0 && strcmp(redirect,  ">") != 0 && strcmp(redirect,  "<") != 0 && strcmp(redirect,  "&>") != 0 && strcmp(redirect,  "&>>") != 0){
		perror("Incorrect redirection syntax");
		return;
	}

	
	if (strcmp(redirect, ">>") == 0){
		*out = fopen(filename, "a");		 		
	} else if (strcmp(redirect, ">") == 0){
		*out = fopen(filename, "w");
	} else if (strcmp(redirect, "&>") == 0){
		*out = fopen(filename, "w");
		*err = *out;	
	} else if (strcmp(redirect, "&>>") == 0){
		*out = fopen(filename, "a");
		*err = *out;
	} else if (strcmp(redirect, "<") == 0){
		*in = fopen(filename, "r");
	}
}

/*
Intializes the global struct "History" that keeps track of used commands
*/
void initHistory(struct History *hist){
	hist->capacity = INIT_HIST_SIZE;
	hist->count = 0;
	hist->history = malloc(hist->capacity * sizeof(char *));
	for (int i = 0; i < hist->capacity; i++){
		hist->history[i] = NULL;
	}
}

/*
Frees the global struct "History" before exiting
*/
void freeHist(){
	for (int i = 0; i < hist->capacity; i++){
		free(hist->history[i]);
	}	
	free(hist->history);
	free(hist);
}

/* 
Handles adding a command to the history. Called everytime a command is executed
*/
void addCommandToHist(char *command, struct History *hist){

	if (hist->count == hist->capacity){
		// Discard the oldest command
		free(hist->history[0]);

		for (int i = 1; i < hist->count; i++){
			hist->history[i-1] = hist->history[i];
		}
		
		// Creates a new pointer to the new command with strdup
		hist->history[hist->count - 1] = strdup(command);
	} else {
		hist->history[hist->count] = strdup(command);
		hist->count++;	
	}

	//printf("Added command to history: %s\n", command);
}

/* 
Handles freeing all local vars before exiting
*/
void freeLocalVars(){
	
	struct localVarNode *curr = shellVarHead;
	struct localVarNode *nxt;
	while (curr != NULL){
		nxt = curr->next;	
		free(curr->varName);
		free(curr->varValue);
		free(curr);
		curr = nxt;
	}
}
/*
Helper function to do quick sort when printing out the history
*/
int strCmp(const void *str1, const void *str2){
	const char *rec1 = *(const char **) str1;
	const char *rec2 = *(const char **) str2;

	return strcmp(rec1, rec2);
}
/*
Handles the built in call "ls"
*/
int lsBuiltIn(FILE *out, FILE *err){
	
	DIR* dir = opendir(".");

	int arrSize = 10;
	char **fileNames = (char **)calloc(arrSize, sizeof(char *));
	if (fileNames == NULL){
		perror("calloc failed");
		exit(1);
	}
	

	if (!dir){
		if (err == NULL){
			perror("ls failed");
		} else {
			fprintf(err, "ls failed");
		}
		return 1;
	} 

	struct dirent *d;

	int fileCount = 0; 

	while ((d = readdir(dir))){
		if (d->d_name[0] == '.'){
			continue;
		}
		//printf("%s\n", d->d_name);
		
		
		if (fileCount < arrSize){
			int size = strlen(d->d_name);
			fileNames[fileCount] = calloc((size + 1), sizeof(char));
			if (fileNames[fileCount] == NULL){
				perror("calloc failed");
				exit(1);
			}
			strncpy(fileNames[fileCount], d->d_name, size);
		} else {	
			int size = strlen(d->d_name);
			arrSize = arrSize * 2;
			fileNames = realloc(fileNames, arrSize * sizeof(char *)); // Realloc size of fileNames arr if array is full	
			fileNames[fileCount] = calloc((size + 1), sizeof(char));
			if (fileNames == NULL || fileNames[fileCount] == NULL){
				perror("realloc or calloc failed");
				exit(1);
			}
			strncpy(fileNames[fileCount], d->d_name, size);
		}
		
		fileCount++;
	} 


	qsort(fileNames, fileCount, sizeof(char *), strCmp);

	
	int i = 0;
	while (i < fileCount){
		if (fileNames[i] != NULL){
			if (out == NULL){
				printf("%s\n", fileNames[i]);
			} else {
				fprintf(out, "%s\n", fileNames[i]);
			}
		}
		i++;
	}

	
	// Free fileNames
	i = 0;
	
	while (i < fileCount){
		free(fileNames[i]);
		i++;
	}
	free(fileNames);
		
	closedir(dir);
	return 0; 
}

/*
Handles the built in call "cd"
*/
int cdBuiltIn(char *path){
	
	char updatePWD[1024];

	if (chdir(path) == -1){
		return 1;
	}

	// Update $PWD environment variable, use getcwd to update
	if (getcwd(updatePWD, sizeof(updatePWD)) == NULL){
		perror("getcwd failed");
		return 1;
	}
	
	if (setenv("PWD", updatePWD, 1) == -1){
		perror("setenv failed");
		return 1;
	} 
	return 0;
}

/*
Handles the built in call "vars"
*/
int varsBuiltIn(struct localVarNode **head, FILE *out, FILE *err){
	struct localVarNode *curr = *head;

	if (curr == NULL){
		if (err == NULL){
			printf("No shell/local variables defined\n");
		} else {
			fprintf(err, "No shell/local variables defined\n");
		}
		return 0;
	}

	while (curr != NULL){
		if (out == NULL){
			printf("%s=%s\n", curr->varName, curr->varValue);
		} else {
			fprintf(out, "%s=%s\n", curr->varName, curr->varValue);
		}
		curr = curr->next;
	}
	return 0;
}

/*
Handles the built in call "local"
*/
int localBuiltIn(char *args[]){
	
	char *tempVar = args[1];

	char *varName;
	char *varVal;

	varName = strtok(tempVar, "=");
	varVal = strtok(NULL, "=");

	if (varVal == NULL){
		varVal = "";
	}

	if (varName == NULL){
		printf("Invalid local variable syntax\n");
		return 1;
	}

	// Replace shell/environment variables
	char *value;
	if (varVal[0] == '$'){
		char *variableName = strtok(varVal, "$");
		value = getenv(variableName);
			
		if (value == NULL){
			//Check if it is a local/shell variable
			struct localVarNode *currNode = shellVarHead;
			while(currNode != NULL){
				if (strcmp(currNode->varName, variableName) == 0){
					varVal = currNode->varValue;
					break;
				}
				currNode = currNode->next;
			}
		} else {
			// It is a environment variable not shell
			varVal = value;
		}
	}

	struct localVarNode *newVar = malloc(sizeof(struct localVarNode));
	struct localVarNode *prev = NULL;
	if (newVar == NULL){
		perror("malloc failed");
		exit(1);
	}

	newVar->varName = strdup(varName);
	newVar->varValue = strdup(varVal);
	newVar->next = NULL;

	// No shell variables declared yet
	if (shellVarHead == NULL){
		shellVarHead = newVar;
		return 0;
	} 

	// Iterate over list unti the next is not NULL
	struct localVarNode *curr = shellVarHead;
	while (curr != NULL){
		if (strcmp(curr->varName, varName) == 0){
			free(curr->varValue);
			curr->varValue = strdup(varVal);
			free(newVar->varName);
			free(newVar->varValue);
			free(newVar);
			return 0;
		} 
		prev = curr;
		curr = curr->next;
	}
	
	prev->next = newVar;


	return 0;	
}

/*
Handles the built in call "export"
*/
int exportBuiltIn(char *args[]){
	char *tempVar = args[1];
	char *varName;
	char *varVal;

	if (args[1] == NULL){
		perror("Missing arguments for export\n");
		return 1;	
	}

	varName = strtok(tempVar, "=");
	varVal = strtok(NULL, "=");

	if (varName == NULL || varVal == NULL){
		perror("Invalid export syntax\n");
		return 1;
	}

	if (setenv(varName, varVal, 1) == -1){
		perror("setenv failed");
		return 1;
	}
	
	return 0;
}

/*
Handles teh built in call "history"
*/
void historyBuiltIn(struct History *hist, char *args[], FILE *out, FILE *err){
	// We are updating the size of the history
	if (args[1] != NULL && strcmp(args[1], "set") == 0){
		if (args[2] == NULL){
			if (err == NULL){
				printf("Syntax: history set <n>\n");
			} else {
				fprintf(err, "Syntax: history set <n>\n");
			}
			return; 
		}

		int setSize = atoi(args[2]);
		if (setSize <= 0){
			if (err == NULL){
				printf("Error: History Size must a positive integer value\n");
			} else {
				fprintf(err, "Error: History Size must a positive integer value\n");
			}
			return;
		}

		if (setSize < hist->count){
			for (int i = setSize; i < hist->count; i++){
				free(hist->history[i]);
			}
			hist->count = setSize;
		}

		hist->history = realloc(hist->history, setSize * sizeof(char*));

		if (hist->history == NULL){
			if (err == NULL){
				printf("Failed to resize history");
			} else {
				fprintf(err, "Failed to resize history");
			}
			exit(1);
		}

		hist->capacity = setSize;
	} else if (args[1] != NULL){ // We are executing a command in the history
		// Need to handle error conditions
		int histIndex = atoi(args[1]);
		if (histIndex > 0 && histIndex <= hist->count){
			char *commandToRun = strdup(hist->history[hist->count - histIndex]);
			executeCommand(commandToRun);
			free(commandToRun);
		}
	} else {
		for (int i = 0; i < hist->count; i++){
			if (hist->history[i] != NULL){
				if (out == NULL){
					printf("%d) %s\n", i + 1, hist->history[hist->count - 1 - i]);
				} else {	
					fprintf(out, "%d) %s\n", i + 1, hist->history[hist->count - 1 - i]);
				}
			}	
		}
	}
}

/*
Handles checking if a command is a Linux command
*/
char *checkCommand(char *command){

	// Get the path environment variable
	char *path = getenv("PATH");
	if (path == NULL){
		perror("getenv failed\n");
		exit(1);
	}

	// Make sure to do bounds checking and free null pointers
	char *fullPath = strdup(path);
	//printf("fullPath: %s\n", fullPath);

	char *currDir = strtok(fullPath, ":");	
	//printf("currDir: %s\n", currDir);	

	while (currDir != NULL){
				
		char *execDir = (char *)malloc(strlen(currDir) + strlen(command) + 2);	
		if (execDir == NULL){
			perror("Memory alloc failed");
			exit(1);
		}
		
		// Have to append the command to the directory
		sprintf(execDir, "%s/%s", currDir, command);	
		
		if (access(execDir, X_OK) == 0){
			free(fullPath);
			return execDir; 
		}
		
		free(execDir);
		currDir = strtok(NULL, ":");
	}	

	free(fullPath);
	return NULL;
}


/*
Handles parsing and executing all commands
*/
int executeCommand(char *inputBuffer){
	const int MAX_TOKENS = 128;
	char *args[MAX_TOKENS];
	char *command;
	int numTokens = 0;

	
	char fileBuffer[1024];	
	char *fileArgs[MAX_TOKENS];
	
	char *addedCommand;

	inputBuffer[strcspn(inputBuffer, "\n")] = '\0';

	addedCommand = strdup(inputBuffer);
	
	args[0] = strtok(inputBuffer, " \t\n");
	command = args[0];

	// Check if first argument is a comment 
	if (args[0] == NULL || args[0][0] == '#'){
		return 0;
	}


	while (args[numTokens] != NULL && numTokens < MAX_TOKENS - 1){
		numTokens++;
		args[numTokens] = strtok(NULL,  " \t\n");
	}

	args[numTokens] = NULL; // Need to null terminate the arguments to pass into execv

	// Parse to determine if redirection is implemented in the command
	char *redirect = NULL;
	char *file = NULL;

	int reDirIndex = -1;
	for (int i = 0; args[i] != NULL; i++){
		if (strstr(args[i], "&>>")){
			redirect = "&>>";
			file = args[i] + 3;
			reDirIndex = i;
			//args[i] = NULL; // Take out the redirection token
			break;
		} else if (strstr(args[i], ">>")){
			int numInd = 1; // The file descriptor for stdin is 1:
			char *reDir = strstr(args[i], ">>");
			if (args[i][0] != '>' && args[i][1] != '>'){
				*reDir = '\0';
				numInd = atoi(args[i]);
			}
			*reDir = '>';
			redirect = ">>";
			stdoutNo = numInd;
			file = reDir + 2;
			reDirIndex = i;
			break;
		} else if (strstr(args[i], "&>")){
			redirect = "&>";
			file = args[i] + 2;
			reDirIndex = i;
			break;
		} else if (strstr(args[i], ">")){
			int numInd = 1; // The file descriptor for stdout is 1 
			char * reDir = strstr(args[i], ">");
			if (args[i][0] != '>'){
				*reDir = '\0';
				numInd = atoi(args[i]);
			}
			*reDir = '>';
			redirect = ">";
			stdoutNo = numInd;
			file = reDir + 1;
			reDirIndex = i;
			break;
		} else if (strstr(args[i], "<")){
			int numInd = 0; // The file descriptor for stdin is 0
			char *reDir = strstr(args[i] , "<");
			if (args[i][0] != '<'){
				*reDir = '\0';
				numInd = atoi(args[i]);
			}
			*reDir = '<';
			redirect = "<";
			stdinNo = numInd;
			file = reDir + 1;
			reDirIndex = i;
			break;
		}
	}

	
	FILE *in = NULL;
	FILE *out = NULL;
	FILE *err = NULL;

	if (redirect != NULL){
		redirections(redirect, file, &in, &out, &err);
	}


	// Replace any shell and environment variables win the command, parses environment and shell variables
	if (redirect == NULL || strcmp(redirect, "<") != 0){	
		numTokens = 0;
		char *value;
		while (args[numTokens] != NULL && numTokens < MAX_TOKENS){
			// Want to parse $ differently if local or export calls used
			if (strcmp(args[0], "local") == 0 && strcmp(args[0], "export")){
				break;
			}
			
			if (args[numTokens][0] == '$'){
				char *variableName = strtok(args[numTokens], "$");
				value = getenv(variableName);
				
				if (value == NULL){
					//Check if it is a local/shell variable
					struct localVarNode *currNode = shellVarHead;
					while(currNode != NULL){
						if (strcmp(currNode->varName, variableName) == 0){
							args[numTokens] = currNode->varValue;
							break;
						}
						currNode = currNode->next;
					}
				} else {
					// It is a environment variable not shell
					args[numTokens] = value;
					break;
				}
			}
			numTokens++;
		}
	} else {
		if (!fgets(fileBuffer, 1024, in)){
			perror("fgets failed");
			return 1; // fgets faiiled change
		}
		
		fileBuffer[strcspn(fileBuffer, "\n")] = '\0';

		fileArgs[0] = command;
		fileArgs[1] = strtok(fileBuffer, " \t\n");

		int k = 1;
		while (fileArgs[k] != NULL && k < MAX_TOKENS){
			k++;
			fileArgs[k] = strtok(NULL,  " \t\n");
		}
			
	}


	// Check if command is a built in command
	int execStatus = 1;	
	if (strcmp(command, "ls") == 0){
		execStatus = lsBuiltIn(out, err);

		free(addedCommand);
		if (out != NULL){
			fclose(out);
			out = NULL;
			err = NULL;
		}
		return 0;
	} else if (strcmp(command, "cd") == 0){
		if (in == NULL){
			execStatus = cdBuiltIn(args[1]);
		} else {
			execStatus = cdBuiltIn(fileArgs[1]);
		}
		free(addedCommand);
		if (in != NULL){
			fclose(in);
			in = NULL;
		}
		if (out != NULL){
			fclose(out);
			out = NULL;
			err = NULL;
		}
		return 0;
	} else if (strcmp(command, "history") == 0){
		if (in == NULL){
			historyBuiltIn(hist, args, NULL, NULL);
		} else {
			historyBuiltIn(hist, fileArgs, out, err);
		}
		
		free(addedCommand);
		
		if (in != NULL){
			fclose(in);
			in = NULL;
		}
		if (out != NULL){
			fclose(out);
			out = NULL;
			err = NULL;
		}
		return 0;
	} else if (strcmp(command, "vars") == 0){
		execStatus = varsBuiltIn(&shellVarHead, out, err);

		free(addedCommand);
		if (out != NULL){
			fclose(out);
			out = NULL;
			err = NULL;
		}
		return 0; 
	} else if(strcmp(command, "export") == 0){
		if (in == NULL){
			execStatus = exportBuiltIn(args);
		} else {
			execStatus = exportBuiltIn(fileArgs);
		}
		free(addedCommand);
		if (out != NULL){
			fclose(out);
			out = NULL;
			err = NULL;
		}
		return 0;
	} else if(strcmp(command, "local") == 0){
		if (in == NULL){
			execStatus = localBuiltIn(args);
		} else {
			execStatus = localBuiltIn(fileArgs);
		}
		free(addedCommand);
		return 0;
	} else if(strcmp(command, "exit") == 0){	
		freeLocalVars();
		free(addedCommand);
		freeHist();
		free(wholeCommand);
		if (out != NULL){
			fclose(out);
			out = NULL;
			err = NULL;
		}
		exit(returnCode);
	}



		
	char *execDir = NULL; 
	// Check if command is an executable to run
	if (command[0] == '/' || command[0] == '.'){
		if (access(command, X_OK) == 0){
			execDir = command;
		} else {
			//printf("Command not found: %s\n", command);
			free(addedCommand);
			free(execDir);
			return -1;
		}
	} else{	// Check if command is a linux command		
		execDir = checkCommand(command);
		if (execDir == NULL){
			//printf("Command not found: %s\n", command);
			free(addedCommand);
			free(execDir);
			//free(args); // New Change
			return -1;
		}
	}
	

	// Free any FDs for built in commands
	
	if (in != NULL){
		fclose(in);
		in = NULL;
	} 
	if (out != NULL){
		fclose(out);
		out = NULL;
	}
	if (err != NULL){
		fclose(err);
		err = NULL;
	} 

	int in_fd, out_fd, err_fd;
	in_fd = -1;
	out_fd = -1;
	err_fd = -1;

	// Create child process of the shell to run the command
	pid_t pid = fork();
	
	int waitStatus;
	
	if (pid == 0){
		redirect_fd(redirect, file, &in_fd, &out_fd, &err_fd);
		if (reDirIndex != -1){
			args[reDirIndex] = 0;
		}	
		if (execv(execDir, args) == -1){
			perror("Execv failed");
		} else {
			execStatus = 0; // execv did not fail
		}
	} else if (pid > 0){
		waitpid(pid, &waitStatus, 0);
		execStatus = 0;
	} else {
		perror("error while executing fork");
	}

	
	// Delete later, fix the return types of the built in functions
	if (execStatus == 1){
		perror("Built in functions not working");
	} 
	
	if (hist->count == 0){ // Check if hist is empty, if so want to add
		addCommandToHist(addedCommand, hist);
	} else if (strcmp(addedCommand, hist->history[hist->count - 1]) != 0){ // Check if current command is the same as the last
		addCommandToHist(addedCommand, hist);
	}


	// Reset file descriptors
	stdoutNo = STDOUT_FILENO;
	stdinNo = STDIN_FILENO;


		
	free(addedCommand);
	free(execDir);
	

	return 0;
}

/*
Handles shell program flow in batch mode
*/
void batchMode(char *file){
	FILE *ptr;
	char *input = NULL; // File stream input
	size_t inputLen = 0;

	hist = malloc(sizeof(struct History));

	// Initialize history		
	initHistory(hist);

	ptr = fopen(file, "r");
	if (ptr == NULL){
		perror("Cannot open file ");
		exit(0);
	}

	/*	
	if (getline(&input, &inputLen, ptr) != -1){
		if (strncmp(input, "#!", 2) == 0){
			// skip, is shebang
		} else {
			fseek(ptr, 0, SEEK_SET); // Go back to beginning of file because it is not a shebang
		}
	} */

	
	while (1){
		if (ptr){
			if (getline(&input, &inputLen, ptr) == -1){
				break; // Failure that means we are at the end of the file
			}	
		}

		wholeCommand = strdup(input);
		
		wholeCommand[strcspn(wholeCommand, "\n")] = '\0';

		// Check if input is empty or just whitespace	
		if (strlen(input) == 0 || strspn(input, " \t\n") == strlen(input)){
			free(wholeCommand);
			continue;
		} 
		
		returnCode = executeCommand(input);	
		
		free(wholeCommand);
	}
}
/*
Handles shell program flow in interactive mode
*/
void interactiveMode(){
	const int BUFFER_SIZE = 1024;

	char inputBuffer[BUFFER_SIZE];


    hist = malloc(sizeof(struct History));



	// Initialize history		
	initHistory(hist);

	while (1){

		printf("wsh> ");
		fflush(stdout);	
		if (fgets(inputBuffer, BUFFER_SIZE, stdin)){
		} else {
			exit(returnCode);
		}
		

		wholeCommand = strdup(inputBuffer);
		
		wholeCommand[strcspn(wholeCommand, "\n")] = '\0';
		
		// Check if input is empty or just whitespace	
		if (strlen(inputBuffer) == 0 || strspn(inputBuffer, " \t\n") == strlen(inputBuffer)){
			free(wholeCommand);
			continue;
		}	
		
		returnCode = executeCommand(inputBuffer);
		free(wholeCommand);
		
	}
}


int main(int argc, char *argv[]){

	if (setenv("PATH", "/bin", 1) != 0){
		perror("setevn error");
	}	
	
	if (argc == 1){
		interactiveMode();
	} else if (argc == 2){
		batchMode(argv[1]);	
    }
}

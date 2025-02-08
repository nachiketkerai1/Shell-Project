#ifndef WSH_H
#define WSH_H

#define INIT_HIST_SIZE 5

struct localVarNode;

struct localVarNode{
	char *varName;
	char* varValue;
	
	struct localVarNode* next;
};


struct History {
	char **history;
	int count;
	int capacity;
};

void historyBuiltIn(struct History *hist, char *args[], FILE *out, FILE *err);
int executeCommand(char *inputBuffer);

#endif

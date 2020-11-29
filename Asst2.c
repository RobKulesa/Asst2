#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

//TODO: Debug globlas
int debugDH = 0;
int debugFH = 0;
int debugTok = 0;
int usingThreads = 1;
int debugJSD = 0;
int debugMain = 0;
//TODO: Structs
typedef struct tokNode{
    char *token;
    int freq;
    double discreteProb;
    struct tokNode *next;
} tokNode;

typedef struct fileNode{
    char* path;
    int tokCount;
    tokNode *tokens;
    struct fileNode* next;
}fileNode;

typedef struct thrdNode{
    pthread_t thread;
    struct thrdNode *next;
}thrdNode;

typedef struct thrdArg{
    char* thrdFilePath;
    fileNode** fileLLHead;
    pthread_mutex_t* mut;
}thrdArg;

//TODO: Function Signatures
void *direcHandler(void *argStruct);
void *fileHandler(void *argStruct);
double jensenShannonDist(fileNode *f1, fileNode *f2);
void freeThrdArg(thrdArg* argStruct);
int goodDirectory(char* path);
int goodFile(char* path);
char *concatPath(char* beg, char* end);
void tokenizeFilePtr(fileNode *ptr);
void fileMergeSort(fileNode** headRef);
fileNode* merge (fileNode *f1, fileNode *f2);
void split(fileNode* src, fileNode** leftPtr, fileNode** rightPtr);
void fixFileName(char* badFilePath);

void *direcHandler(void *argStruct) {
    //1
    thrdArg* args = (thrdArg*) malloc(sizeof(argStruct));
    args->mut = ((thrdArg*) argStruct)->mut;
    args->fileLLHead = ((thrdArg*) argStruct)->fileLLHead;
    args->thrdFilePath = ((thrdArg*) argStruct)->thrdFilePath;
    if(debugDH) printf("direcHandler | %s:\tInitiate\n", args->thrdFilePath);
    if(!goodDirectory(args->thrdFilePath)) {
        printf("Error: direcHandler: %s is an invalid directory path.\n", args->thrdFilePath);
        return (void *)1;
    }
    pthread_t* threadArr;
    int thrdIndex;
    threadArr = NULL;
    thrdIndex = -1;
    DIR* thrdDirec = opendir(args->thrdFilePath);
    struct dirent *thrdDirent;
    while((thrdDirent = readdir(thrdDirec)), thrdDirent!=NULL){
        if(strcmp(thrdDirent->d_name, ".") && strcmp(thrdDirent->d_name, "..") && strcmp(thrdDirent->d_name, ".DS_Store")) {
            if(debugDH) printf("direcHandler | %s:\td_name is %s\n", args->thrdFilePath,(thrdDirent->d_name));
            //3a
            if(thrdDirent->d_type == DT_DIR) {
                //3aI
                if(debugDH) printf("direcHandler | %s:\t->Path found is directory\n",args->thrdFilePath);
                thrdArg *newThrdArg = (thrdArg *)malloc(sizeof(thrdArg));
                if(threadArr == NULL){
                    threadArr = (pthread_t*)malloc(sizeof(pthread_t));
                    thrdIndex++;
                } else {
                    pthread_t* temp = threadArr;
                    threadArr = (pthread_t*)malloc(sizeof(pthread_t)*(thrdIndex+2));
                    memcpy(threadArr, temp, (size_t)(sizeof(pthread_t)*(thrdIndex+1)));
                    free(temp);
                    thrdIndex++;
                }
                newThrdArg->mut = args->mut;
                newThrdArg->fileLLHead = args->fileLLHead;
                newThrdArg->thrdFilePath = concatPath(args->thrdFilePath, thrdDirent->d_name);
                if(debugDH) printf("direcHandler | %s:\t->Path before calling direcHandler on it: %s\n",args->thrdFilePath, newThrdArg->thrdFilePath);
                pthread_create(threadArr+thrdIndex, NULL, direcHandler,newThrdArg);
                if(debugDH) printf("direcHandler | %s:\t->Path after calling direcHandler on it: %s\n",args->thrdFilePath, newThrdArg->thrdFilePath);
                
            } else if(thrdDirent->d_type == DT_REG) {
                if(debugDH) printf("direcHandler | %s:\t->Path found is a normal file\n",args->thrdFilePath);
                thrdArg *newThrdArg = (thrdArg *)malloc(sizeof(thrdArg));                
                if(threadArr == NULL){
                    threadArr = (pthread_t*)malloc(sizeof(pthread_t));
                    thrdIndex++;
                } else{
                    pthread_t* temp = threadArr;
                    threadArr = (pthread_t*)malloc(sizeof(pthread_t)*(thrdIndex+2));
                    if(debugDH) printf("direcHandler | %s:\t->Path found is a normal file\n",args->thrdFilePath);

                    memcpy(threadArr, temp, (size_t)(sizeof(pthread_t)*(thrdIndex+1)));
                    free(temp);
                    thrdIndex++;
                }
                newThrdArg->mut = args->mut;
                newThrdArg->fileLLHead = args->fileLLHead;
                newThrdArg->thrdFilePath = concatPath(args->thrdFilePath, thrdDirent->d_name);

                if(debugDH) printf("direcHandler | %s:\t->Path before calling fileHandler on it: %s\n",args->thrdFilePath, newThrdArg->thrdFilePath);
                pthread_create(threadArr+thrdIndex, NULL, fileHandler,newThrdArg);
                if(debugDH) printf("direcHandler | %s:\t->Path after calling fileHandler on it: %s\n",args->thrdFilePath, newThrdArg->thrdFilePath);
            }
        }
    
    }
    int i;
    for(i = 0; i < thrdIndex+1; i++){
        pthread_join(threadArr[i],NULL);
    }
    free(threadArr);
    closedir(thrdDirec);
    //5
    if(debugDH) printf("direcHandler | %s:\tFINISH\n", args->thrdFilePath);
    freeThrdArg(args);
    if(debugDH) printf("direcHandler |:\tFINISH2\n");
    return (void *)0;
}

/* Function: fileHandler
 * Algorithm
 * 1. Examine the file and make sure it is accessible
 *      IF not, out put error and "return gracefully", else, proceed
 * 2. LOCK mutex
 * 3. Add entry to shared data structure
 *      - Should contain file path AND token count
 * 4. UNLOCK mutex
 * 5. ANALYZE and TOKENIZE file
 *      - Use insertion sort as new tokens are added to the list
 *      - Proceed to calculate the discrete probability of each token
 * 7. Free the argStruct
 */
void *fileHandler(void *argStruct){
    //1
    
    thrdArg *args = (thrdArg *)argStruct;
    pthread_mutex_lock(args->mut);
    if (debugFH) printf("\tfileHandler | %s:\tINITIATE\n", args->thrdFilePath);
    if(!goodFile(args->thrdFilePath)) {
        pthread_mutex_unlock(args->mut);
        return (void *)1;
    }
    fileNode* ptr;
    fileNode** headPtrPtr = args->fileLLHead;
    if (debugFH) printf("\tfileHandler | %s:\tTestline 1\n", args->thrdFilePath);
    if(*(headPtrPtr) == NULL){
        *(headPtrPtr) = (fileNode*)malloc(sizeof(fileNode));
        ptr = *headPtrPtr;
        if (debugFH) printf("\tfileHandler | %s:\tTestline 2\n", args->thrdFilePath);
    } else{
        ptr = *headPtrPtr;
        if (debugFH) printf("\tfileHandler | %s:\tTestline 3\n", args->thrdFilePath);
        while(ptr->next != NULL){
            if (debugFH) printf("\tfileHandler | %s:\tTestline 4\n", args->thrdFilePath);
            ptr = ptr->next;
        }

        ptr->next = (fileNode*)malloc(sizeof(fileNode));
        if (debugFH) printf("\tfileHandler | %s:\tTestline 7\n", args->thrdFilePath);
        ptr = ptr->next;
    }
    ptr->next = NULL;
    ptr->tokCount = 0;
    ptr->tokens = NULL;
    ptr->path = (char*)malloc(strlen(args->thrdFilePath)+1);
    strcpy(ptr->path, args->thrdFilePath);

    

    //5
    if (debugFH) printf("\tfileHandler | %s:\tCalling Tokenizer\n", args->thrdFilePath);
    tokenizeFilePtr(ptr);
    
    //6
    

    if(debugFH) printf("\tfileHandler | %s:\tFINISH\n", args->thrdFilePath);
    pthread_mutex_unlock(args->mut);
    
    //freeThrdArg(args);

    
    
    return (void *)0;
}

void tokenizeFilePtr(fileNode *ptr){
    if(debugTok) printf("\t\ttokenizeFilePtr | %s: INITIATE\n", ptr->path);
    //Pull data out of the file
    if(debugTok) printf("\t\ttokenizeFilePtr | %s: Open\n", ptr->path);
    int fd = open(ptr->path, O_RDWR);
    if(debugTok) printf("\t\ttokenizeFilePtr | %s: lseek\n", ptr->path);
    int fileSize = (int)lseek(fd, (size_t)0, SEEK_END);
    lseek(fd, (off_t)0, SEEK_SET);
    if(debugTok) printf("\t\ttokenizeFilePtr | %s: Allocating Buffer\n", ptr->path);
    char *buffer = (char *)malloc(fileSize);
    //buffer[fileSize] = '\0';
    if(debugTok) printf("\t\ttokenizeFilePtr | %s: Reading info into buffer\n", ptr->path);
    read(fd, (void *)buffer, (size_t)fileSize);
    char* currentTok = (char*)malloc(fileSize); 
    int tokCount = 0;

    //Iterate through buffer
    if(debugTok) printf("\t\ttokenizeFilePtr | %s: Starting Tokenization Process\n", ptr->path);
    tokNode *tokPtrCurr = ptr->tokens;
    tokNode *tokPtrPrev;
    int i = 0;
    int tokIndex = 0;
    if(debugTok) printf("\t\ttokenizeFilePtr | %s: Starting for loop\n", ptr->path);
    for(i = 0; i < fileSize; i++){
        if(debugTok) printf("\t\ttokenizeFilePtr | %s: Start FORLOOP Iteration [%d]\n", ptr->path, i);
        tokPtrCurr = ptr->tokens;
        if(debugTok) printf("\t\ttokenizeFilePtr | %s: Start FORLOOP Iteration [%d]1\n", ptr->path, i);
        tokPtrPrev = NULL;
        if(debugTok) printf("\t\ttokenizeFilePtr | %s: Start FORLOOP Iteration [%d]2\n", ptr->path, i);
        //printf("\t\ttokPtrCurr is at position %p\n", tokPtrCurr);
        //Skip white space in the beginning
        tokIndex = 0;
        while(i < fileSize && isspace(buffer[i]))
            i++;
        //Parse until the next white space is found
        while(i < fileSize && !isspace(buffer[i])){
            if(isalpha(buffer[i]) || buffer[i] == '-'){
                currentTok[tokIndex] = tolower(buffer[i]);
                tokIndex++;
            }
            i++;
        }
        if(debugTok) printf("\t\ttokenizeFilePtr | %s: Start FORLOOP Iteration [%d]3\n", ptr->path, i);
        //End the token with a null terminator
        currentTok[tokIndex] = '\0';
        if(debugTok) printf("\t\ttokenizeFilePtr | %s: Start FORLOOP Iteration [%d]4\n", ptr->path, i);
        //printf("\t\tDETECTION:: %s\n", currentTok);

        while(tokPtrCurr!=NULL){

            if(ptr->path == NULL) printf("Null thing found1\n");
            if(currentTok == NULL) printf("Null thing found2\n");
            if(tokPtrCurr->token == NULL) printf("Null thing found2\n");
            
            if(debugTok) printf("\t\ttokenizeFilePtr | %s: Comparing [%s] and [%s]\n", ptr->path, currentTok, tokPtrCurr->token);
            //printf("\t\t\tComparing to: %s\n", tokPtrCurr->token);
            if(debugTok) printf("\t\ttokenizeFilePtr | len of currentTok is: %lu\n", strlen(currentTok));
            if(strcmp(currentTok, tokPtrCurr->token) <= 0){
                if(debugTok) printf("\t\ttokenizeFilePtr | error here?5\n");
                break;
            }
                
            else{
                if(debugTok) printf("\t\ttokenizeFilePtr | error here?\n");
                tokPtrPrev = tokPtrCurr;
                if(debugTok) printf("\t\ttokenizeFilePtr | error here2?\n");
                tokPtrCurr = tokPtrCurr->next;
                if(debugTok) printf("\t\ttokenizeFilePtr | error here?3\n");
            }
            if(debugTok) printf("\t\ttokenizeFilePtr | error here?4\n");
        }
        
        if(debugTok) printf("\t\ttokenizeFilePtr | %s: Preparing for Inserting Token [%s]\n", ptr->path, currentTok);
        if(debugTok) printf("\t\ttokenizeFilePtr | %s: Inserting Token\n", ptr->path);
        if(tokPtrCurr == NULL && tokPtrPrev == NULL){
            if(debugTok) printf("\t\ttokenizeFilePtr | %s: Token List for given FilePtr is unitialized\n", ptr->path);
            ptr->tokens = (tokNode *)malloc(sizeof(tokNode));
            ptr->tokens->token = (char *)malloc(strlen(currentTok)+1);
            strcpy(ptr->tokens->token, currentTok);
            ptr->tokens->freq = 1;
            //printf("\t\t\tINSERTION: %s\n", ptr->tokens->token);
        }  else if(tokPtrPrev!= NULL && tokPtrCurr == NULL){//We have reached the end of a nonempty list
            if(debugTok) printf("\t\ttokenizeFilePtr | %s: End of List reached\n", ptr->path);
            tokPtrPrev->next = (tokNode *)malloc(sizeof(tokNode));
            tokPtrPrev->next->token = (char *)malloc(strlen(currentTok)+1);
            strcpy(tokPtrPrev->next ->token, currentTok);
            tokPtrPrev->next->freq = 1;
        } else if(tokPtrPrev == NULL && tokPtrCurr != NULL && strcmp(currentTok, tokPtrCurr->token) != 0 ){
            if(debugTok) printf("\t\ttokenizeFilePtr | %s: Insert in beginning of list\n", ptr->path);
            tokNode *temp = ptr->tokens;
            ptr->tokens = (tokNode *)malloc(sizeof(tokNode));
            ptr->tokens->token = (char *)malloc(strlen(currentTok)+1);
            ptr->tokens->freq = 1;
            strcpy(ptr->tokens->token, currentTok);
            ptr->tokens->next = temp;
        } else if(tokPtrCurr!=NULL && strcmp(currentTok, tokPtrCurr->token) == 0){
            if(debugTok) printf("\t\ttokenizeFilePtr | %s: Equal Token Found\n", ptr->path);
            tokPtrCurr->freq += 1;
        }else if(tokPtrCurr != NULL && tokPtrPrev!=NULL){
            if(debugTok) printf("\t\ttokenizeFilePtr | %s: Inserting somewhere in the middle of the list\n", ptr->path);
            tokPtrPrev->next = (tokNode *)malloc(sizeof(tokNode));
            if(debugTok) printf("\t\ttokenizeFilePtr | %s: Test1\n", ptr->path);
            tokPtrPrev->next->token = (char *)malloc(strlen(currentTok)+1);
            if(debugTok) printf("\t\ttokenizeFilePtr | %s: Test2\n", ptr->path);
            strcpy(tokPtrPrev->next ->token, currentTok);
            if(debugTok) printf("\t\ttokenizeFilePtr | %s: Test3\n", ptr->path);
            tokPtrPrev->next->next = tokPtrCurr;
            if(debugTok) printf("\t\ttokenizeFilePtr | %s: Test4\n", ptr->path);
            tokPtrPrev->next->freq = 1;
            if(debugTok) printf("\t\ttokenizeFilePtr | %s: Test5\n", ptr->path);
        }
        tokCount++;
        if(debugTok) printf("\t\ttokenizeFilePtr | %s: Test6\n", ptr->path);
    }
    if(debugTok) printf("\t\ttokenizeFilePtr | %s: Calculating Discrete Probabilities\n", ptr->path);
    tokNode *tokPtrTemp;
    ptr->tokCount =tokCount;
    for(tokPtrTemp = ptr->tokens; tokPtrTemp!=NULL; tokPtrTemp = tokPtrTemp->next){
        if(debugTok) printf("\t\ttokenizeFilePtr | %s: Calculating Discrete Probability for %s\n", ptr->path, tokPtrTemp->token);
            if(tokPtrTemp)
            tokPtrTemp->discreteProb = (double)tokPtrTemp->freq / (double)tokCount;
    }
    if(debugTok) printf("\t\ttokenizeFilePtr | %s: freeing currentTok\n", ptr->path);
    free(currentTok);
    if(debugTok) printf("\t\ttokenizeFilePtr | %s: free currentTok\n", ptr->path);
    if(debugTok) printf("\t\tFINISHTOKENIZER: %s\n", ptr->path);
    return;
}
/* Function: fileMergeSort
 * Purpose: Executes Merge Sort on the file data structure to sort the files by token Count
 */
void fileMergeSort(fileNode** headRef) {
    fileNode* headPtr = *headRef;
    fileNode* ptr1;
    fileNode* ptr2;
    if(headPtr == NULL || headPtr->next == NULL) {
        return;
    }
    split(headPtr, &ptr1, &ptr2);
    fileMergeSort(&ptr1); fileMergeSort(&ptr2);
    *headRef = merge(ptr1, ptr2);
}

void split(fileNode* src, fileNode** leftPtr, fileNode** rightPtr) {
    fileNode* fast;
    fileNode* slow;
    slow = src;
    fast = src->next;
    while(fast != NULL) { 
		fast = fast->next; 
		if(fast != NULL) { 
			slow = slow->next; 
			fast = fast->next; 
		} 
	}
    *leftPtr = src; 
	*rightPtr = slow->next; 
	slow->next = NULL; 
    return;
}

fileNode* merge(fileNode* f1, fileNode* f2) {
    fileNode* result = NULL; 
	if (f1 == NULL) 
		return (f2); 
	else if (f2 == NULL) 
		return (f1); 
	if (f1->tokCount <= f2->tokCount) { 
		result = f1; 
		result->next = merge(f1->next, f2); 
	} 
	else { 
		result = f2; 
		result->next = merge(f1, f2->next); 
	} 
	return result; 
}
/* Function: concatPath
 * Purpose: takes a filepath and a filename and concatenates them properly
 */
char *concatPath(char* beg, char* end) {
    int noSlash = beg[strlen(beg)-1] != '/';
    int newLen = strlen(beg)+strlen(end);
    if(noSlash)
        newLen++;
    char* newPath = (char *)malloc(newLen);
    strcat(newPath, beg);
    if(noSlash)
        strcat(newPath, "/");
    strcat(newPath, end);
    newPath[newLen] = '\0';
    return newPath;
}


void fixFileName(char* badFileName){
    if(strlen(badFileName) <= 0)
        return;
    int i;
    for(i = 0; i < strlen(badFileName)-4; i++){
        if(badFileName[i]== '.' && badFileName[i+1]=='t' && badFileName[i+2]=='x' && badFileName[i+3] == 't'){
            badFileName[i+4] = '\0';
        }
    }
    return;
}

void printDataStruct(fileNode** headPtr){
    fileNode *ptr = *headPtr;
    fileNode *fTemp;
    tokNode *ptr2;
    tokNode *tokTemp;
    
    while(ptr!=NULL){
        double combinedProb = 0.0;
        if(ptr->path != NULL){
            printf("FILE: %s\tTokCount = %d\n", ptr->path, ptr->tokCount);
        }
        ptr2 = ptr->tokens;
        while(ptr2!=NULL){
            printf("\tToken: %s\tFrequency: %d\tProbability: %f\n", ptr2->token, ptr2->freq, ptr2->discreteProb);
            combinedProb+=ptr2->discreteProb;
            //free(ptr2->token);
            tokTemp = ptr2;
            ptr2 = ptr2->next;
            //free(tokTemp);
        }
        printf("\tCOMBINED PROB: %f\n", combinedProb);
        //free(ptr->path);
        fTemp = ptr;
        ptr = ptr->next;
        //free(fTemp);
    }
}

//TODO: Freeing Functions
void freeThrdArg(thrdArg* argStruct){
    free(argStruct->thrdFilePath);
    free(argStruct);
}

void freeDatastructure(fileNode** headPtr){
    fileNode *ptr = *headPtr;
    fileNode *fTemp;
    tokNode *ptr2;
    tokNode *tokTemp;
    
    while(ptr!=NULL){
        double combinedProb = 0.0;
        if(ptr->path != NULL){
            printf("FILE: %s\tTokCount = %d\n", ptr->path, ptr->tokCount);
        }
        ptr2 = ptr->tokens;
        while(ptr2!=NULL){
            combinedProb+=ptr2->discreteProb;
            free(ptr2->token);
            tokTemp = ptr2;
            ptr2 = ptr2->next;
            free(tokTemp);
        }
        //printf("\tCOMBINED PROB: %f\n", combinedProb);
        if(ptr->path!=NULL)
            free(ptr->path);
        fTemp = ptr;
        ptr = ptr->next;
        free(fTemp);
    }
    
    free(headPtr);
    return;
}

//? VALIDITY CHECK FUNCTIONS
/*
 * Function: goodDirectory
 * Returns 1 if the argument is a valid directory name and 0 otherwise
 */
int goodDirectory(char* path){
    //printf("\t**Path is: %s\n", path);
    DIR* pathDir = opendir(path);
    //printf("->*Opened the directory\n");
    int ret = 0;
    if(pathDir != NULL)
        ret = 1;
    //printf("\t**Closing the directory..\n");
    closedir(pathDir);
    return ret;
}

/*
 * Function: goodFile
 * Returns 1 if the argument is a valid file and 0 otherwise
 */
int goodFile(char* path){
    return (open(path, O_RDONLY) > 0);
}

double jensenShannonDist(fileNode *f1, fileNode *f2){
    //Initialize Pointers
    if(debugJSD) printf("JSD | Initiating\n");
    tokNode *meanHead = NULL;
    tokNode *f1Ptr = f1->tokens;
    tokNode *f2Ptr = f2->tokens;
    if(debugJSD) printf("JSD | Finished assigning f1Ptr  and f2Ptrs\n");
    //Iterate through fileNodes to create mean token list
    while(f1Ptr != NULL && f2Ptr != NULL) {
        if(debugJSD) printf("JSD | strcmp: [%s] and [%s]\n", f1Ptr->token,f2Ptr->token);
        if(strcmp(f1Ptr->token, f2Ptr->token) == 0) { //In the case where f1 and f2 point to tokens of equal value
            if(meanHead == NULL){
                if(debugJSD) printf("JSD | Mean List declaration because it doesn't exist\n");
                meanHead = (tokNode *)malloc(sizeof(tokNode));
                meanHead->token = f1Ptr->token;
                meanHead->discreteProb = (f1Ptr->discreteProb + f2Ptr->discreteProb) / 2.0;
                meanHead->next = NULL;
            } else{
                tokNode *meanPtr = meanHead;
                while(meanPtr->next != NULL){
                    meanPtr = meanPtr->next;
                }
                if(debugJSD) printf("JSD | Iterating until we find the last Mean Node\n");
                meanPtr->next = (tokNode *)malloc(sizeof(tokNode));
                meanPtr->next->token = f1Ptr->token;
                meanPtr->next->discreteProb = (f1Ptr->discreteProb + f2Ptr->discreteProb) / 2.0;
                meanPtr->next->next = NULL;
            }
            f1Ptr = f1Ptr->next;
            f2Ptr = f2Ptr->next;
        } else if(strcmp(f1Ptr->token, f2Ptr->token) < 0){
            if(meanHead == NULL){
                meanHead = (tokNode *)malloc(sizeof(tokNode));
                meanHead->token = f1Ptr->token;
                meanHead->discreteProb = (f1Ptr->discreteProb) / 2.0;
                meanHead->next = NULL;
            } else{
                tokNode *meanPtr = meanHead;
                while(meanPtr->next != NULL){
                    meanPtr = meanPtr->next;
                }
                meanPtr->next = (tokNode *)malloc(sizeof(tokNode));
                meanPtr->next->token = f1Ptr->token;
                meanPtr->next->discreteProb = (f1Ptr->discreteProb) / 2.0;
                meanPtr->next->next = NULL;
            }
            f1Ptr = f1Ptr->next;
        } else {
            if(meanHead == NULL){
                meanHead = (tokNode *)malloc(sizeof(tokNode));
                meanHead->token = f2Ptr->token;
                meanHead->discreteProb = (f2Ptr->discreteProb) / 2.0;
                meanHead->next = NULL;
            } else{
                tokNode *meanPtr = meanHead;
                while(meanPtr->next != NULL){
                    meanPtr = meanPtr->next;
                }
                meanPtr->next = (tokNode *)malloc(sizeof(tokNode));
                meanPtr->next->token = f2Ptr->token;
                meanPtr->next->discreteProb = (f2Ptr->discreteProb) / 2.0;
                meanPtr->next->next = NULL;
            }
            f2Ptr = f2Ptr->next;
        }
    }
    
    if(f1Ptr!=NULL && f2Ptr == NULL){
        
        tokNode *meanPtr = meanHead;
        while(meanPtr->next != NULL)
            meanPtr = meanPtr->next;
        while(f1Ptr!=NULL){
            meanPtr->next = (tokNode *)malloc(sizeof(tokNode));
            meanPtr->next->token = f1Ptr->token;
            meanPtr->next->discreteProb = (f1Ptr->discreteProb) / 2.0;
            meanPtr = meanPtr->next;
            f1Ptr = f1Ptr->next;
        }
        meanPtr->next = NULL;
    } else if(f1Ptr == NULL && f2Ptr != NULL){
        tokNode *meanPtr = meanHead;
        while(meanPtr->next != NULL)
            meanPtr = meanPtr->next;
        while(f2Ptr!=NULL){
            meanPtr->next = (tokNode *)malloc(sizeof(tokNode));
        meanPtr->next->token = f2Ptr->token;
        meanPtr->next->discreteProb = (f2Ptr->discreteProb) / 2.0;
        meanPtr = meanPtr->next;
        f2Ptr = f2Ptr->next;
        }
        meanPtr->next = NULL;
    }



    tokNode* t1 = meanHead;
    if(debugJSD) printf("Mean Token List:\t");
    while(t1){
            if(debugJSD) printf("[  %s | %d | %f  ]", t1->token, t1->freq, t1->discreteProb);
            if(t1->next){
               if(debugJSD)  printf("->\n");
            }
            t1=t1->next;
            
    }
    if(debugJSD) printf("\n");
    double KLDF1 = 0;
    f1Ptr = f1->tokens;
    tokNode* meanPtr = meanHead;    
    while(f1Ptr!=NULL){
        if(debugJSD) printf("f1Ptr->token: %s\tmeanPtr->token: %s\n", f1Ptr->token, meanPtr->token);
        if(strcmp(f1Ptr->token, meanPtr->token) > 0){
            meanPtr = meanPtr->next;
        } else {
            if(debugJSD) printf("\t%f * log(%f/%f)  =  %f\n", f1Ptr->discreteProb, f1Ptr->discreteProb, meanPtr->discreteProb, f1Ptr->discreteProb * (log10(f1Ptr->discreteProb / meanPtr->discreteProb)));
            KLDF1 += f1Ptr->discreteProb * (log10(f1Ptr->discreteProb / meanPtr->discreteProb));
            meanPtr = meanPtr->next;
            f1Ptr = f1Ptr->next;
        }
    }
    if(debugJSD) printf("KLD1: %f\n", KLDF1);

    double KLDF2 = 0;
    f2Ptr = f2->tokens;
    meanPtr = meanHead;    
    while(f2Ptr!=NULL && meanPtr != NULL){
        if(debugJSD) printf("f2Ptr->token: %s\tmeanPtr->token: %s\n", f2Ptr->token, meanPtr->token);
        if(strcmp(f2Ptr->token, meanPtr->token) > 0){
            meanPtr = meanPtr->next;
        } else {
           if(debugJSD) printf("\t%f * log(%f/%f)  =  %f\n", f2Ptr->discreteProb, f2Ptr->discreteProb, meanPtr->discreteProb, f2Ptr->discreteProb * (log10(f2Ptr->discreteProb / meanPtr->discreteProb)));
            KLDF2 += f2Ptr->discreteProb * (log10(f2Ptr->discreteProb / meanPtr->discreteProb));
            meanPtr = meanPtr->next;
            f2Ptr = f2Ptr->next;
            
        }
    }

    //printf("Not skipping: here are the calculations:\n");
    if(debugJSD) printf("KLD2: %f\n", KLDF2);



    return (KLDF1+KLDF2)/2;
}


/* Function: main
 * Algorithm
 * 1. Check if the commandline directory is valid/accessible
 *      If not, return. IF SO, proceed
 * 2. Initialize and allocate mutex
 * 3. Initialize shared data structure
 * 4. Initialize and pass argStruct into threading function that will run Directory handler
 * 5. Once the call has finished, examine the data structure
 *      IF the shared data structue is empty, return error. If not, proceed
 * 6. Sort the files by token count
 *      IF the shared data structure only has one entry, emit a warning and stop
 * 7. Analyze and compute Jensen Shannon Distance for each file pair
 * 8. Free everything allocated by the main function
 *      1. The first argStruct
 *      2. The shared datastructure
 *      3. The mutex
 */
int main(int argc, char** argv) {
    
    char buf[PATH_MAX];
    char *res = realpath(argv[1], buf);
    if(res) {
        if(debugMain) printf("This source is at %s.\n", buf);
    } else {
        perror("realpath");
        exit(EXIT_FAILURE);
    }

    if(!goodDirectory(buf)){
        printf("Argument contains invalid directory. Error.\n");
        return 0;
    }
    if(debugMain)printf("Starting step 2\n");
    //2. 
    pthread_mutex_t *mutx = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(mutx, NULL);
    
    if(debugMain)printf("Starting step 3\n");
    //3. NOTE: headPTR does not correspond with any file.
    fileNode** headPtr = (fileNode**)malloc(sizeof(fileNode));

    if(debugMain)printf("Starting step 4\n");
    //4.
    thrdArg* args = (thrdArg *)malloc(sizeof(thrdArg));
    args->fileLLHead = headPtr;
    args->mut = mutx;
    *(args->fileLLHead) = NULL;
    args->thrdFilePath = (char*)malloc(strlen(buf)+1);
    strcpy(args->thrdFilePath, buf);
    if(debugMain)printf("Starting step 5\n");
    //5. 
    direcHandler((void *)args);
    if((*headPtr)->next == NULL){
        printf("Error: Nothing detected\n");
        return 1;
    }

    if(debugMain) printf("Starting step 6\n");
    //6.
    if((*headPtr)->next == NULL){
        printf("Warning: Only one regular file was detected!\n");
        return 1;
    }
    fileMergeSort(headPtr); //its that easy
    if(debugMain)printDataStruct(headPtr);


    if(debugMain) printf("Starting step 7\n");
    //7.
    fileNode* dsPtr;
    fileNode* dsPtr2;
    //TODO fix this for loop kek
    for(dsPtr = *headPtr; dsPtr->next != NULL; dsPtr = dsPtr->next) {
        for(dsPtr2 = dsPtr->next; dsPtr2!=NULL; dsPtr2 = dsPtr2->next){
            if(debugJSD) printf("Attempting JSD on: %s AND \t%s\n", dsPtr->path, dsPtr2->path);
            double jsd = jensenShannonDist(dsPtr, dsPtr2);
            if(jsd > 0.3)
                printf("\033[0m");
            else if(jsd > 0.25)
                printf("\033[0;34m");
            else if(jsd > 0.2)
                printf("\033[0;36m");
            else if(jsd > 0.15)
                printf("\033[0;32m");
            else if(jsd > 0.1)
                printf("\033[0;33m");
            else
                printf("\033[0;31m");
            printf("%f", jsd);
            printf("\033[0m \"%s\" and \"%s\"\n", dsPtr->path, dsPtr2->path);
        }
    } 
    if(debugMain)printf("Started step 8\n");
    //8.
    free(mutx);
    freeDatastructure(headPtr);
    

    return 0;



}

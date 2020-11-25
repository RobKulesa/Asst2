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
    struct fileNode *next;
}fileNode;

typedef struct thrdNode{
    pthread_t thread;
    struct thrdNode *next;
}thrdNode;

typedef struct thrdArg{
    char* thrdFilePath;
    fileNode *fileLLHead;
    pthread_mutex_t *mut;
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
void fileMergesort(fileNode** headRef);
fileNode* merge (fileNode *f1, fileNode *f2);
void split(fileNode* src, fileNode** leftPtr, fileNode** rightPtr);


//TODO: HUGE FUNCTIONS
/* Function: direcHandler
 * Instructions
 * 1. Check the directory to see if it is accessible
 *      IF not, output error and "return gracefully". IF so, proceed
 * 2. Open the directory with opendir() and iterate through it with readdir
 * 3. IF entry is a directory
 *      -> create new pthread and run another copy of direcHandler on the directory
 * 4. IF entry is a file
 *      -> create a new pthread and run another copy of file handler
 * 5. IF entry is not directory or normal file
 *      -> ignore it
 * 6. Join all threads.
 * 
 * ALGORITHM
 * 1. Check directory validity. If invalid, stop and error; IF valid proceed.
 * 2. Initialize thrdLL headPtr
 * 3. Iterate through the dirent structure
 *      a. If Directory
 *          I. grab the name of the directory
 *          II. malloc a new string that is the concatenation of the name and the current path
 *          III. pass that concatenation, mutex, and fileLL headptr as a struct arg to a new thread
 *          IV. Hold that thread in a new thrdNode by iterating to the end of the LL
 *      b. If File (same as above, but with file)
 *      c. If garbage -> do nothing
 * 4. Free threadLL
 * 5. Free the threadArg that was passed to the function
 */
void *direcHandler(void *argStruct){
    //1
    thrdArg *args = (thrdArg *)argStruct;
    printf("direcHandler: Starting direcHandler on %s\n", args->thrdFilePath);
    if(!goodDirectory(args->thrdFilePath)) {
        //printf("Error: direcHandler: %s is an invalid directory path.\n", args->thrdFilePath);
        return (void *)1;
    }
    //2
    int thrdCount = 0;
    pthread_t* threadArr = NULL;
    pthread_t* tempPtr;
    //thrdNode *thrdHead = (thrdNode *)malloc(sizeof(thrdNode));
    //thrdHead->next = NULL;
    //3
    //thrdNode *ptr1;
    //thrdNode *ptr2;
    DIR* thrdDirec = opendir(args->thrdFilePath);
    struct dirent *thrdDirent;
    while((thrdDirent = readdir(thrdDirec)), thrdDirent!=NULL){
        if(strcmp(thrdDirent->d_name, ".") && strcmp(thrdDirent->d_name, "..") && strcmp(thrdDirent->d_name, ".DS_Store")){
            printf("direcHandler: Looking at path: %s\n", thrdDirent->d_name);
            printf("direcHandler: thrdCount is %d in thread for %s\n", thrdCount, args->thrdFilePath);

            //3a
            if(thrdDirent->d_type == DT_DIR) {
                //3aI
                printf("direcHandler: ->Path found is directory\n");
                thrdArg *newThrdArg = (thrdArg *)malloc(sizeof(thrdArg));
                newThrdArg->mut = args->mut;
                newThrdArg->fileLLHead = args->fileLLHead;
                newThrdArg->thrdFilePath = concatPath(args->thrdFilePath, thrdDirent->d_name);
                printf("direcHandler: result of concatenation of strings %s and %s:\n\t%s\n", args->thrdFilePath, thrdDirent->d_name, newThrdArg->thrdFilePath);
                /*
                ptr1 = thrdHead;
                while(ptr1->next!=NULL)
                    ptr1 = ptr1->next;
                ptr1->next = (thrdNode *)malloc(sizeof(thrdNode));
                ptr1 = ptr1->next;
                */
                threadArr = (pthread_t*)realloc(threadArr, (thrdCount+1)*sizeof(pthread_t));

                pthread_create(threadArr+thrdCount, NULL, direcHandler,newThrdArg);
                thrdCount++;
            } else if(thrdDirent->d_type == DT_REG) {
                printf("direcHandler: ->Path found is a normal file\n");
                thrdArg *newThrdArg = (thrdArg *)malloc(sizeof(thrdArg));
                newThrdArg->mut = args->mut;
                newThrdArg->fileLLHead = args->fileLLHead;
                newThrdArg->thrdFilePath = concatPath(args->thrdFilePath, thrdDirent->d_name);
                printf("direcHandler: result of concatenation of strings %s and %s:\n\t%s\n", args->thrdFilePath, thrdDirent->d_name, newThrdArg->thrdFilePath);
                /*
                ptr1 = thrdHead;
                while(ptr1->next!=NULL)
                    ptr1 = ptr1->next;
                ptr1->next = (thrdNode *)malloc(sizeof(thrdNode));
                ptr1 = ptr1->next;
                */
                threadArr = (pthread_t*)realloc(threadArr, (thrdCount+1)*sizeof(pthread_t));
                //tempPtr = (pthread_t*)malloc((thrdCount+1)*sizeof(pthread_t));
                //memcpy(threadArr, tempPtr, thrdCount*sizeof(pthread_t));
                pthread_create(threadArr+thrdCount, NULL, fileHandler, (void*) newThrdArg);
                thrdCount++;
            }
        }
    
    }

    //4
    int i;
    printf("direcHandler: Joining threads now\n");
    for(i = 0; i < thrdCount; i++){
        pthread_join(threadArr[i], NULL);
    }
    
    //ptr1 = thrdHead->next;
    //free(thrdHead);
    /*
    while(ptr1 != NULL){
        printf("direcHandler: joining thread %d\n", i+1);
        ptr2 = ptr1;
        if(ptr2->thread != NULL)
            pthread_join((ptr2->thread), NULL);
        ptr1 = ptr1->next;
        if(ptr2 != NULL) free(ptr2);
        i++;
    }
    */
    //5
    //*printf("DirecHandler FINISH %s\n", args->thrdFilePath);   
    free(args);
    free(threadArr);
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
    printf("\tExecuting fileHandler on file: %s\n", args->thrdFilePath);
    if(!goodFile(args->thrdFilePath))
        return (void *)1;
    //2
    pthread_mutex_lock(args->mut);

    //3
    fileNode *ptr = args->fileLLHead;
    while(ptr->next != NULL)
        ptr = ptr->next;
    ptr->next = (fileNode *)malloc(sizeof(fileNode));
    ptr = ptr->next;
    ptr->next = NULL;
    ptr->tokCount = 0;
    ptr->path = (char *)malloc(strlen(args->thrdFilePath)+1); //+1 to add space for null terminator
    strcpy(ptr->path, args->thrdFilePath);

    //4
    pthread_mutex_unlock(args->mut);

    //5
    tokenizeFilePtr(ptr);

    //6
    //*printf("\tFreeing thread args%s\n", ptr->path);
    freeThrdArg(args);

    //printf("\tFINISH FILEHANDLER %s\n", ptr->path);
    return (void *)0;
}

/* Function: Tokenize File
 * 1. Look at file
 * 2. Add tokens using insertion sort
 * 3. Keep total token count as we proceed
 * 4. Then compute the discrete probability of each token
 */
void tokenizeFilePtr(fileNode *ptr){
    //printf("\t\tExecuting tokenizer on %s\n", ptr->path);
    //Pull data out of the file
    int fd = open(ptr->path, O_RDWR);
    int fileSize = (int)lseek(fd, (size_t)0, SEEK_END);
    lseek(fd, (off_t)0, SEEK_SET);
    char *buffer = (char *)malloc(fileSize);
    //buffer[fileSize] = '\0';
    read(fd, (void *)buffer, (size_t)fileSize);
    char currentTok[fileSize];
    int tokCount = 0;

    //Iterate through buffer
    tokNode *tokPtrCurr = ptr->tokens;
    tokNode *tokPtrPrev;
    int i;
    int tokIndex;
    for(i = 0; i < fileSize; i++){
        tokPtrCurr = ptr->tokens;
        tokPtrPrev = NULL;
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
        //End the token with a null terminator
        currentTok[tokIndex] = '\0';
        //printf("\t\tDETECTION:: %s\n", currentTok);

        while(tokPtrCurr!=NULL){
            //printf("\t\t\tComparing to: %s\n", tokPtrCurr->token);
            if(strcmp(currentTok, tokPtrCurr->token) <= 0){
                //printf("\t\t\tstrcmp: %d\n", strcmp(currentTok, tokPtrCurr->token));
                break;
            }
                
            else{
                //printf("\t\t\tstrcmp: %d.\n", strcmp(currentTok, tokPtrCurr->token));
                tokPtrPrev = tokPtrCurr;
                tokPtrCurr = tokPtrCurr->next;
            }
                
        }
        
        /*
        if(tokPtrPrev == NULL && tokPtrCurr == NULL){
            printf("\t\tBoth pointers are NULL\n");
        } else if(tokPtrPrev == NULL && tokPtrCurr != NULL){
            printf("\t\tPrev is null, but curr is not\n");
        } else if(tokPtrPrev!= NULL && tokPtrCurr == NULL){
            printf("\t\tPrev isn't null, but curr is\n");
        } else{
            printf("\t\tneither are null\n");
        }
        */

        if(tokPtrCurr == NULL && tokPtrPrev == NULL){
            //printf("\t\tCASE1: token list unitialized.\n");
            ptr->tokens = (tokNode *)malloc(sizeof(tokNode));
            ptr->tokens->token = (char *)malloc(strlen(currentTok)+1);
            strcpy(ptr->tokens->token, currentTok);
            ptr->tokens->freq = 1;
            //printf("\t\t\tINSERTION: %s\n", ptr->tokens->token);
        }  else if(tokPtrPrev!= NULL && tokPtrCurr == NULL){//We have reached the end of a nonempty list
            //printf("\t\tWe have reached the end of a nonempty list\n");
            tokPtrPrev->next = (tokNode *)malloc(sizeof(tokNode));
            tokPtrPrev->next->token = (char *)malloc(strlen(currentTok)+1);
            strcpy(tokPtrPrev->next ->token, currentTok);
            tokPtrPrev->next->freq = 1;
        } else if(tokPtrPrev == NULL && tokPtrCurr != NULL && strcmp(currentTok, tokPtrCurr->token) != 0 ){
            //printf("\t\tNeed to insert new node at beginning of list\n");
            tokNode *temp = ptr->tokens;
            ptr->tokens = (tokNode *)malloc(sizeof(tokNode));
            ptr->tokens->token = (char *)malloc(strlen(currentTok)+1);
            ptr->tokens->freq = 1;
            strcpy(ptr->tokens->token, currentTok);
            ptr->tokens->next = temp;
        } else if(tokPtrCurr!=NULL && strcmp(currentTok, tokPtrCurr->token) == 0){
            //printf("\t\tCASE: equal token found\n");
            tokPtrCurr->freq += 1;
        }else if(tokPtrCurr != NULL && tokPtrPrev!=NULL){
            //printf("\t\tNeed to insert new node at middle of list\n");
            tokPtrPrev->next = (tokNode *)malloc(sizeof(tokNode));
            tokPtrPrev->next->token = (char *)malloc(strlen(currentTok)+1);
            strcpy(tokPtrPrev->next ->token, currentTok);
            tokPtrPrev->next->next = tokPtrCurr;
            tokPtrPrev->next->freq = 1;
        }
        tokCount++;
    }
    tokNode *tokPtrTemp;
    ptr->tokCount =tokCount;
    for(tokPtrTemp = ptr->tokens; tokPtrTemp!=NULL; tokPtrTemp = tokPtrTemp->next){
        tokPtrTemp->discreteProb = (double)tokPtrTemp->freq / (double)tokCount;
    }
    printf("\t\tFINISHTOKENIZER: %s\n", ptr->path);
    return;
}


/* Function: jensenShannonDist
 * 1. Create Mean token list
 * 2. Compute mean probabilities of any tokens that are the same using formula
 * 3. Compute Kullbeck-Leibler Divergence of each distribution
 * 4. Use KLD to find JSD
 */

double jensenShannonDist(fileNode *f1, fileNode *f2){
    //Initialize Pointers
    tokNode *meanHead = NULL;
    tokNode *f1Ptr = f1->tokens;
    tokNode *f2Ptr = f2->tokens;
    
    //Iterate through fileNodes to create mean token list
    while(f1Ptr != NULL && f2Ptr != NULL) {
        printf("\t\t*****Fucky wucky token: %s\n", f2Ptr->token);
        if(strcmp(f1Ptr->token, f2Ptr->token) == 0) { //In the case where f1 and f2 point to tokens of equal value
            if(meanHead == NULL){
                meanHead = (tokNode *)malloc(sizeof(tokNode));
                meanHead->token = f1Ptr->token;
                meanHead->discreteProb = (f1Ptr->discreteProb + f2Ptr->discreteProb) / 2.0;
            } else{
                tokNode *meanPtr = meanHead;
                while(meanPtr->next != NULL){
                    meanPtr = meanPtr->next;
                }
                meanPtr->next = (tokNode *)malloc(sizeof(tokNode));
                meanPtr->next->token = f1Ptr->token;
                meanPtr->next->discreteProb = (f1Ptr->discreteProb + f2Ptr->discreteProb) / 2.0;
            }
            f1Ptr = f1Ptr->next;
            f2Ptr = f2Ptr->next;
        } else if(strcmp(f1Ptr->token, f2Ptr->token) < 0){
            if(meanHead == NULL){
                meanHead = (tokNode *)malloc(sizeof(tokNode));
                meanHead->token = f1Ptr->token;
                meanHead->discreteProb = (f1Ptr->discreteProb) / 2.0;
            } else{
                tokNode *meanPtr = meanHead;
                while(meanPtr->next != NULL){
                    meanPtr = meanPtr->next;
                }
                meanPtr->next = (tokNode *)malloc(sizeof(tokNode));
                meanPtr->next->token = f1Ptr->token;
                meanPtr->next->discreteProb = (f1Ptr->discreteProb) / 2.0;
            }
            f1Ptr = f1Ptr->next;
        } else {
            if(meanHead == NULL){
                meanHead = (tokNode *)malloc(sizeof(tokNode));
                meanHead->token = f2Ptr->token;
                meanHead->discreteProb = (f2Ptr->discreteProb) / 2.0;
            } else{
                tokNode *meanPtr = meanHead;
                while(meanPtr->next != NULL){
                    meanPtr = meanPtr->next;
                }
                meanPtr->next = (tokNode *)malloc(sizeof(tokNode));
                meanPtr->next->token = f2Ptr->token;
                meanPtr->next->discreteProb = (f2Ptr->discreteProb) / 2.0;
            }
            f2Ptr = f2Ptr->next;
        }
    }


    //Clean up step: If one list became null before the other
    if(f1Ptr!=NULL && f2Ptr == NULL){
        tokNode *meanPtr = meanHead;
        while(meanPtr->next != NULL)
            meanPtr = meanPtr->next;
        meanPtr->next = (tokNode *)malloc(sizeof(tokNode));
        meanPtr->next->token = f1Ptr->token;
        meanPtr->next->discreteProb = (f1Ptr->discreteProb) / 2.0;
    } else if(f1Ptr == NULL && f2Ptr != NULL){
        tokNode *meanPtr = meanHead;
        while(meanPtr->next != NULL)
            meanPtr = meanPtr->next;
        meanPtr->next = (tokNode *)malloc(sizeof(tokNode));
        meanPtr->next->token = f2Ptr->token;
        meanPtr->next->discreteProb = (f2Ptr->discreteProb) / 2.0;
    }
    
    //Calculate the Kullbeck-Leibler Divergence of both files
    int KLDF1 = 0;
    f1Ptr = f1->tokens;
    tokNode* meanPtr = meanHead;    
    while(f1Ptr!=NULL){
        if(strcmp(f1Ptr->token, meanPtr->token) > 0){
            f1Ptr = f1Ptr->next;
        } else {
            KLDF1 += f1Ptr->discreteProb * (log(f1Ptr->discreteProb / meanPtr->discreteProb));
            meanPtr = meanPtr->next;
            f1Ptr = f1Ptr->next;
        }
    }
    int KLDF2 = 0;
    f2Ptr = f2->tokens;
    meanPtr = meanHead;    
    while(f2Ptr!=NULL){
        if(strcmp(f2Ptr->token, meanPtr->token) > 0){
            f2Ptr = f2Ptr->next;
        } else {
            KLDF2 += f2Ptr->discreteProb * (log(f2Ptr->discreteProb / meanPtr->discreteProb));
            meanPtr = meanPtr->next;
            f2Ptr = f2Ptr->next;
        }
    }


    return (KLDF1 + KLDF2)/2;  
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
    return newPath;
}


void printDataStruct(fileNode *headPtr){
    fileNode *ptr = headPtr;
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

void freeDatastructure(fileNode *headPtr){
    fileNode *ptr = headPtr;
    fileNode *fTemp;
    tokNode *ptr2;
    tokNode *tokTemp;
    
    while(ptr!=NULL){
        double combinedProb = 0.0;
        if(ptr->path != NULL){
            //printf("FILE: %s\tTokCount = %d\n", ptr->path, ptr->tokCount);
        }
        ptr2 = ptr->tokens;
        while(ptr2!=NULL){
            //printf("\tToken: %s\tFrequency: %d\tProbability: %f\n", ptr2->token, ptr2->freq, ptr2->discreteProb);
            combinedProb+=ptr2->discreteProb;
            free(ptr2->token);
            tokTemp = ptr2;
            ptr2 = ptr2->next;
            free(tokTemp);
        }
        //printf("\tCOMBINED PROB: %f\n", combinedProb);
        free(ptr->path);
        fTemp = ptr;
        ptr = ptr->next;
        free(fTemp);
    }
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
int main(int argc, char** argv){    
    
    
    //1.
    if(!goodDirectory(argv[1])){
        printf("Argument contains invalid directory. Error.\n");
        return 0;
    }
    printf("Starting step 2\n");
    //2. 
    pthread_mutex_t *mutx = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(mutx, NULL);
    
    printf("Starting step 3\n");
    //3. NOTE: headPTR does not correspond with any file.
    fileNode *headPtr = (fileNode *)malloc(sizeof(fileNode));
    headPtr->next = NULL;

    printf("Starting step 4\n");
    //4.
    thrdArg* args = (thrdArg *)malloc(sizeof(thrdArg));
    args->fileLLHead = headPtr;
    args->mut = mutx;
    args->thrdFilePath = argv[1];

    printf("Starting step 5\n");
    //5. 
    direcHandler((void *)args);
    if(headPtr->next == NULL){
        printf("Error: Nothing detected\n");
        return 1;
    }
    fileNode *temp = headPtr;
    headPtr = headPtr->next;
    free(temp);

    printf("Starting step 6\n");
    //6.
    if(headPtr->next == NULL){
        printf("Warning: Only one regular file was detected!\n");
        return 1;
    }
    fileMergeSort(&headPtr); //its that easy
    printDataStruct(headPtr);


    printf("Starting step 7\n");
    //7.
    fileNode* dsPtr;
    fileNode* dsPtr2;
    
    //TODO fix this for loop kek
    for(dsPtr = headPtr; dsPtr->next != NULL; dsPtr = dsPtr->next) {
        for(dsPtr2 = dsPtr->next; dsPtr2!=NULL; dsPtr2 = dsPtr2->next){
            //printf("Attempting JSD on: %s AND \t%s\n", dsPtr->path, dsPtr2->path);
            //double jsd = jensenShannonDist(dsPtr, dsPtr2);
            //printf("%f %s %s\n", jsd ,dsPtr->path, dsPtr2->path);
        }
    } 
    printf("Started step 8\n");
    //8.
    free(mutx);
    freeDatastructure(headPtr);
    

    //printf("%s, %d\n", concatPath("Hello/", "yark.txt"), (int)strlen(concatPath("Hello/", "yark.txt")));
    return 0;
}
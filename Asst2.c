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
#include<Ctype.h>
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
    if(!goodDirectory(args->thrdFilePath)){
        printf("Error: direcHandler: %s is an invalid directory path.\n", args->thrdFilePath);
        return (void *)1;
    }
    //2
    thrdNode *thrdHead = (thrdNode *)malloc(sizeof(thrdNode));
    thrdHead->next = NULL;
    //3
    thrdNode *ptr1;
    thrdNode *ptr2;
    DIR* thrdDirec = opendir(args->thrdFilePath);
    struct dirent *thrdDirent;
    while((thrdDirent = readdir(thrdDirec)), thrdDirent!=NULL){
        if(strcmp(thrdDirent->d_name, ".") && strcmp(thrdDirent->d_name, "..") && strcmp(thrdDirent->d_name, ".DS_Store")){
            printf("Looking at Directory: %s\n", thrdDirent->d_name);
            //3a
            if(thrdDirent->d_type == DT_DIR){
                //3aI
                thrdArg *newThrdArg = (thrdArg *)malloc(sizeof(thrdArg));
                newThrdArg->mut = args->mut;
                newThrdArg->fileLLHead = args->fileLLHead;
                newThrdArg->thrdFilePath = concatPath(args->thrdFilePath, thrdDirent->d_name);
                
                ptr1 = thrdHead;
                while(ptr1->next!=NULL)
                    ptr1 = ptr1->next;
                ptr1->next = (thrdNode *)malloc(sizeof(thrdNode));
                ptr1 = ptr1->next;
                pthread_create(&(ptr1->thread), NULL, direcHandler,newThrdArg);
                
            } else if(thrdDirent->d_type == DT_REG){
                thrdArg *newThrdArg = (thrdArg *)malloc(sizeof(thrdArg));
                newThrdArg->mut = args->mut;
                newThrdArg->fileLLHead = args->fileLLHead;
                newThrdArg->thrdFilePath = concatPath(args->thrdFilePath, thrdDirent->d_name);
                
                ptr1 = thrdHead;
                while(ptr1->next!=NULL)
                    ptr1 = ptr1->next;
                ptr1->next = (thrdNode *)malloc(sizeof(thrdNode));
                ptr1 = ptr1->next;
                pthread_create(&(ptr1->thread), NULL, fileHandler,newThrdArg);
            }
        }
            
    }

    //4
    ptr1 = thrdHead;
    while(ptr1 != NULL){
        ptr2 = ptr1;
        if(ptr2->thread != NULL)
            pthread_join((ptr2->thread), NULL);
        ptr1 = ptr1->next;
        free(ptr2);
    }

    //5
    
    free(args);
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
    if(!goodFile(args->thrdFilePath))
        return (void *)1;
    printf("Executing fileHanlder on file: %s\n", args->thrdFilePath);
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
    freeThrdArg(args);

    return (void *)0;
}

/* Function: Tokenize File
 * 1. Look at file
 * 2. Add tokens using insertion sort
 * 3. Keep total token count as we proceed
 * 4. Then compute the discrete probability of each token
 */
void tokenizeFilePtr(fileNode *ptr){
    printf("Executing tokenizer on %s\n", ptr->path);
    //Pull data out of the file
    int fd = open(ptr->path, O_RDWR);
    int fileSize = (int)lseek(fd, (size_t)0, SEEK_END);
    char *buffer = (char *)malloc(fileSize+1);
    //buffer[fileSize] = '\0';
    int err = read(fd, (void *)buffer, (size_t)fileSize);
    printf("FD: %d, fileSize: %d, Read: %d, Buffer: %s\n", fd, fileSize, err, buffer);
    char currentTok[fileSize];
    int tokCount = 0;

    //Iterate through buffer
    tokNode *tokPtrCurr = ptr->tokens;
    tokNode *tokPtrTemp;
    int i;
    int tokIndex;
    for(i = 0; i < fileSize; i++){
        tokPtrCurr = ptr->tokens;
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
        printf("\t Token: %s\n", currentTok);
        //Proceed to insert
        while(tokPtrCurr!=NULL){
            if(strcmp(currentTok, tokPtrCurr->token) >= 0)
                break;
            else
                tokPtrCurr = tokPtrCurr->next;
        }

        if(tokPtrCurr==NULL){
            tokPtrCurr = (tokNode *)malloc(sizeof(tokNode));
            tokPtrCurr->token = malloc(strlen(currentTok)+1);
            strcpy(tokPtrCurr->token, currentTok);
        } else if(strcmp(currentTok, tokPtrCurr->token) == 0){
            tokPtrCurr->freq += 1;
        } else if(strcmp(currentTok, tokPtrCurr->token) > 0){
            tokPtrTemp = tokPtrCurr->next;
            tokPtrCurr->next = (tokNode *)malloc(sizeof(tokNode));
            tokPtrCurr->next->freq = 1;
            tokPtrCurr->next->token = malloc(strlen(currentTok)+1);
            strcpy(tokPtrCurr->next->token, currentTok);
            tokPtrCurr->next->next = tokPtrTemp;
        }
        tokCount++;
    }
    ptr->tokCount =tokCount;
    for(tokPtrTemp = ptr->tokens; tokPtrTemp!=NULL; tokPtrTemp = tokPtrTemp->next){
        tokPtrTemp->discreteProb = tokPtrTemp->freq / tokCount;
    }
    return;
}


/* Function: jensenShannonDist
 * 1. Create Mean token list
 * 2. Compute mean probabilities of any tokens that are the same using formula
 * 3. Compute Kullbeck-Leibler Divergence of each distribution
 * 4. Use KLD to find JSD
 */
double jensenShannonDist(fileNode *f1, fileNode *f2);

/* Function: concatPath
 * Purpose: takes a filepath and a filename and concatenates them properly
 */
char *concatPath(char* beg, char* end){
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
        ptr2 = ptr->tokens;
        while(ptr2!=NULL){
            printf("%s\t", ptr2->token);
            free(ptr2->token);
            tokTemp = ptr2;
            ptr2 = ptr2->next;
            free(tokTemp);
        }
        if(ptr->path != NULL){
            printf("<---- FILE: %s\n", ptr->path);
        }
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
    DIR* pathDir = opendir(path);
    int ret = 0;
    if(pathDir != NULL)
        ret = 1;
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
    
    //2. 
    pthread_mutex_t *mutx = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(mutx, NULL);

    //3. NOTE: headPTR does not correspond with any file.
    fileNode *headPtr = (fileNode *)malloc(sizeof(fileNode));
    headPtr->next = NULL;

    //4.
    thrdArg* args = (thrdArg *)malloc(sizeof(thrdArg));
    args->fileLLHead = headPtr;
    args->mut = mutx;
    args->thrdFilePath = argv[1];

    //5. 

    direcHandler((void *)args);

    //8.
    free(mutx);
    freeDatastructure(headPtr);

    //!TESTING
    /*
    printf("\n*******TESTING KEKEKEKEKEKEKEKEK*********\n");
    char* testStr = "TestingFunction/";
    int i;
    for(i = 0; i < strlen(testStr); i++){
        printf("%c\n", testStr[i]);
    }
    printf("%c is the slash\n", testStr[strlen(testStr)-1]);
    
    printf("ERROR CHECK\n");
    char* test1 = "testing/";
    char* test2 = "123";
    char *test = concatPath(test1, test2);
    printf("concatPath produces: %s\n", test);
    test1 = "zzb";
    test2 = "zza";

    printf("str1: %s, str2: %s, strcmp: %d, \n", test1, test2, strcmp(test1,test2));

    char testArr[100];
    for(i = 0; i < 10; i++){
        testArr[i] = 'a' + i;
    }
    testArr[i] = '\0';
    testArr[i+1] = 'a';
    printf("str1 is: %s and is len: %d\n", testArr, strlen(testArr));

    free(test);
    */

    return 0;
    
}
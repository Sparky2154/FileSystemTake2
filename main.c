

#ifndef FUSE_USE_VERSION
	#define FUSE_USE_VERSION 30
#endif
#include <fuse.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>	// ENOMEM
#include <unistd.h>

//Notice: I used the following filesystem as a template at first and modified it to fit my needs.
// https://github.com/osxfuse/fuse


typedef struct files{
    char* name;
    unsigned long location;
    unsigned long content;
    struct files *nextFile;
    struct stat attr;
}Files;

typedef struct directory{
    char* name;
    unsigned long location;
    struct directory* parent;
    unsigned long parentLocation;
    struct directory* next;
    unsigned long nextLocation;
    struct directory* children;
    unsigned long childrenLocation;
    Files *files;
    unsigned long filesLocation;
    struct stat attr;
} Directory;

void readBytes(void** content, FILE** save, unsigned long *location){
    unsigned long errorCheck = fread(content, 1, 1, *save);
    (*location) = (*location)+1;
    if(errorCheck != 1){
        abort();
    }
}

Directory root;
char* saveState;

Directory *readDirectory(unsigned long location){
    FILE* save = fopen(saveState, "br");
    void* content = malloc(1);
    unsigned long currentLocation = location;
    fsetpos(save, (fpos_t*)&location);

    Directory *readingDirectory = malloc(sizeof(Directory));
    readingDirectory->location = location;

    //reading the name
    char* name = malloc(255);
    readBytes(content, &save, &currentLocation);
    for (int i = 0; i < 255 && (*((char *) content)) != 0; ++i) {
        name[i] = (*((char *) content));
        readBytes(content, &save, &currentLocation);
    }
    readingDirectory->name = name;

    readBytes(content, &save, &currentLocation);
    readingDirectory->childrenLocation = (*((unsigned int*)content));
    readBytes(content, &save, &currentLocation);
    readingDirectory->parentLocation = (*((unsigned int*)content));
    readBytes(content, &save, &currentLocation);
    readingDirectory->nextLocation = (*((unsigned int*)content));
    readBytes(content, &save, &currentLocation);
    readingDirectory->filesLocation = (*((unsigned int*)content));
    readBytes(content, &save, &currentLocation);
    readingDirectory->attr = (*((struct stat*)content));

    return readingDirectory;
}

Files *getFiles(unsigned long location){
    Files *readingFiles = malloc(sizeof(Files));
    //todo
    return readingFiles;
}

Directory* getToFile(const char* path){
    Directory* currentDirectory = root.children;
    int pathIndex = 0;
    int currentNameIndex;
    char* currentName = malloc(255);

    for (int i = 1; path[i] != 0 && i < 255; ++i) {

        //Copying the name of the next directory to be searched
        if (path[i] == '/'){
            currentName[i] = 0;
            currentNameIndex++;
        } else if(path[i] == 0){
            free(currentName);
            return currentDirectory;
        }
        else{
            currentName[i] = path[i];
            currentNameIndex++;
            continue;
        }


        if (currentNameIndex > 0){
            currentNameIndex = 0;

            //Iterate through the directories
            while (strcmp(currentDirectory->name, currentName) != 0){
                if (currentDirectory->next != NULL) {
                    currentDirectory = currentDirectory->next;
                } else if(currentDirectory->nextLocation != 1){
                    currentDirectory->next = readDirectory(currentDirectory->nextLocation);
                    currentDirectory = currentDirectory->next;
                } else{
                    free(currentName);
                    return NULL;
                }
            }

            if(currentDirectory->children != NULL) {
                currentDirectory = currentDirectory->children;
            } else if(currentDirectory->childrenLocation != 1){
                currentDirectory->children = readDirectory(currentDirectory->childrenLocation);
                currentDirectory = currentDirectory->children;
            } else{
                return NULL;
            }

        } else{
            free(currentName);
            return NULL;
        }
    }
    free(currentName);
    return currentDirectory;
}

Files* getFile(const char* path){
    //todo check this
    Directory *currentDirectory = getToFile(path);
    Files *currentFile = currentDirectory->files;
    char *currentName = malloc(255);
    int nameIndex = 0;
    int pathIndex = 0;

    while (nameIndex < 255 && currentName[nameIndex] != 0){
        if(currentName[nameIndex] == '/'){
            nameIndex = 0;
        } else{
            currentName[nameIndex] = path[pathIndex];
            nameIndex++;
        }
        pathIndex++;
    }
    currentName[nameIndex] = 0;

    while (strcmp(currentFile->name, currentName) != 0){
        if (currentFile->nextFile == NULL)
            return NULL;
        currentFile = currentFile->nextFile;
    }
    free(currentName);
    return currentFile;
}



//Start of operations ****************************************************





int fuseGetAttr( const char *path, struct stat *stateBuff ) {
    return 0;
}



int fuseReadDir( const char *path, void *buff, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
    filler( buff, ".", NULL, 0 ); 	// Current Directory
	filler( buff, "..", NULL, 0 ); 	// Parent Directory

	Directory* currentDirectory = getToFile(path);


    return 0;
}

int fuseRead( const char *path, char *buff, size_t size, off_t offset, struct fuse_file_info *fi ) {
    return 0;
}

int fuseWrite(const char *path, const char *buff, size_t size, off_t offset, struct fuse_file_info *fi) {
    Files* file = getFile(path);

    return size;
}

int fuseMkdir(const char* path, mode_t mode){
    getToFile(path);
    return 0;
}


//End of operations *****************************************************

static struct fuse_operations operations = {
//  .getattr	= ,
    .readdir	= fuseReadDir,
//  .read		= ,
	.write		= fuseWrite,
//	.open		= ,
//	.release	= ,
//	.flush		= ,
//	.truncate 	= ,     ???
//	.ftruncate 	= ,     ???
	.mkdir      = fuseMkdir,
};

int main( int argc, char *argv[] ) {
	root.name = "";
	root.location = 0;
	root.childrenLocation = 0;
	root.children = readDirectory(root.childrenLocation);
	saveState = argv[0];

	char* argv2[argc-1];
    for (int i = 1; i < argc; ++i) {
        argv2[i-1] = argv[i];
    }
	return fuse_main( argc-1, argv2, &operations, NULL );
}

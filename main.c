

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
    char* content;
    struct files *nextFile;
    struct stat *attr;
}Files;

typedef struct directory{
    char* name;
    struct directory* parent;
    struct directory* next;
    struct directory* children;
    Files *files;
    struct stat *attr;
} Directory;

Directory root;


Directory* getToFile(const char* path){
    Directory* currentDirectory = root.children;
    int pathIndex = 0;
    int currentNameIndex = 0;
    char* currentName = malloc(255);

    if(strcmp(path, "/") == 0){
        return &root;
    }

    for (int i = 1; path[i] != 0 && i < 255; ++i) {

        //Copying the name of the next directory to be searched
        if (path[i] == '/'){
            currentName[currentNameIndex] = 0;
            currentNameIndex++;
        } else if(path[i] == 0){
            free(currentName);
            return currentDirectory;
        }
        else{
            currentName[currentNameIndex] = path[i];
            currentNameIndex++;
            continue;
        }


        if (currentNameIndex > 0){
            currentNameIndex = 0;

            //Iterate through the directories
            while (strcmp(currentDirectory->name, currentName) != 0){
                if (currentDirectory->next != NULL) {
                    currentDirectory = currentDirectory->next;
                } else{
                    free(currentName);
                    return NULL;
                }
            }

            if(currentDirectory->children != NULL) {
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
    return currentDirectory->parent;
}

Files* getFile(const char* path){
    Directory *currentDirectory = getToFile(path)->parent;
    if(currentDirectory == NULL || currentDirectory->files == NULL){
        return NULL;
    }
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


int fuseRead (const char *path, char *buff, size_t size, off_t offset, struct fuse_file_info *fi ){
    printf("read\n");
    fflush(stdout);
    Files *currentFile = getFile(path);
    memcpy(buff, currentFile->content + offset, size);
    return 0;
}

int fuseWrite(const char *path, const char *buff, size_t size, off_t offset, struct fuse_file_info *fi) {
    printf("write\n");
    fflush(stdout);
    Files *file = getFile(path);
    if(file->content != NULL){
        free(file->content);
    }
    char* content = malloc(size);
    memcpy(content, buff, size);
    file->content = content;

    return 0;
}

int fuseReadDir( const char *path, void *buff, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
    printf("readDir\n");
    fflush(stdout);
    filler( buff, ".", NULL, 0 ); 	// Current Directory
	filler( buff, "..", NULL, 0 ); 	// Parent Directory



	Directory* currentDirectory = getToFile(path)->children;
    Files *files = NULL;
	if(currentDirectory->files != NULL) {
        files = currentDirectory->files;
    }

    while (currentDirectory != NULL && currentDirectory->name != NULL && strcmp(currentDirectory->name, "") != 0){
        filler( buff, currentDirectory->name, NULL, 0 );
        currentDirectory = currentDirectory->next;
    }


    while (files != NULL && files->name != NULL){
        filler( buff, files->name, NULL, 0 );
        files = files->nextFile;
    }

    return 0;
}


int fuseMkdir(const char* path, mode_t mode){
    printf("mkdir\n");
    fflush(stdout);
    Directory *dir = getToFile(path);
    Directory *parent = dir;

    char* name = malloc(255);
    int nameIndex = 0;

    for (int i = 0; i < 255 && path[i] != 0; ++i) {
        name[nameIndex] = path[i];
        if(path[i] == '/'){
            nameIndex = 0;
        }
    }
    if(dir->children == NULL){
        dir->children = malloc(sizeof(Directory));
        dir = dir->children;
    } else{
        dir = dir->children;
        while (dir->next != NULL){
            dir = dir->next;
        }
    }

    dir->parent = parent;
    dir->name = name;
    dir->next = NULL;
    dir->files = NULL;
    dir->children = NULL;
    dir->attr = NULL;
    return 0;
}

int fuseGetattr( const char *path, struct stat *stateBuff ) {
    printf("fun getattr\n");
    fflush(stdout);
    stateBuff->st_uid = getuid();
    stateBuff->st_gid = getgid();
    stateBuff->st_atime = stateBuff->st_mtime = stateBuff->st_ctime = time(NULL);

    Files *currentFile = getFile(path);

    if ( strcmp( path, "/" ) == 0 ) { // Info For The mount point
        stateBuff->st_mode = S_IFDIR | 0755;
        stateBuff->st_nlink = 2;
    }
    else if (currentFile != NULL){ // Info For The req file
        printf("No1");
        stateBuff->st_mode = S_IFREG | 0644;
        stateBuff->st_nlink = 1;
        stateBuff->st_size = strlen(currentFile->content);
    } else
        return -ENOENT;

    return 0;
}

//End of operations *****************************************************

static struct fuse_operations operations = {
    .getattr	= fuseGetattr,
    .readdir	= fuseReadDir,
    .read		= fuseRead,
	.write		= fuseWrite,
	.mkdir      = fuseMkdir,
};

int main( int argc, char *argv[] ) {
    printf("Started\n");
	root.name = "";
	return fuse_main( argc, argv, &operations, NULL );
}

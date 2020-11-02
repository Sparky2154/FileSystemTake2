

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

typedef enum fileType{
    directory,
    file
}FileType;

struct fileDescriptor;

typedef struct files{
    char* content;
    struct stat *attr;
}Files;

typedef struct directory{
    struct fileDescriptor *files;
    struct fileDescriptor *parent;
    struct stat *attr;
} Directory;

typedef struct fileDescriptor{
    FileType type;
    char* name;
    Files *file;
    Directory *directory;
    struct fileDescriptor *next;
    Directory *parent;
}FileDescriptor;


Directory root;


Directory *getToFile(const char* path){
    FileDescriptor *currentFD = root.files;
    int currentNameIndex = 0;
    char currentName[255];

    if (currentFD == NULL){
        return &root;
    }

    if(strcmp(path, "/") == 0){
        return &root;
    }

    for (int i = 1; i < 255; ++i) {

        //Copying the name of the next directory to be searched
        if (path[i] == '/'){
            currentName[currentNameIndex] = 0;
        } else if(path[i] == 0){
            if (currentFD == NULL){
                return NULL;
            }
            return currentFD->parent;
        }
        else{
            currentName[currentNameIndex] = path[i];
            currentNameIndex++;
            continue;
        }


        if (currentNameIndex > 0){
            currentNameIndex = 0;

            //Iterate through the directories
            while (strcmp(currentFD->name, currentName) != 0){
                if (currentFD->next != NULL) {
                    currentFD = currentFD->next;
                } else{
                    return NULL;
                }
            }

            //Go into the directory found
            if(currentFD->type == directory && currentFD->directory != NULL) {
                currentFD = currentFD->directory->files;
            } else{
                return NULL;
            }
        } else{
            return NULL;
        }
    }

    if (currentFD == NULL){
        return NULL;
    }

    return currentFD->directory;
}

char* getName(const char* path){
    char *currentName = malloc(255);
    int nameIndex = 0;

    for (int pathIndex = 0; nameIndex < 255 && path[pathIndex] != 0; pathIndex++){
        if(path[pathIndex] == '/'){
            nameIndex = 0;
        } else{
            currentName[nameIndex] = path[pathIndex];
            nameIndex++;
        }
    }
    currentName[nameIndex] = 0;
    return currentName;
}

Files* getFile(const char* path){
    Directory *currentDirectory = getToFile(path);

    if(currentDirectory == NULL || currentDirectory->files == NULL){
        return NULL;
    }

    FileDescriptor *currentFile = currentDirectory->files;
    char *currentName = getName(path);

    while (strcmp(currentFile->name, currentName) != 0){
        if (currentFile->next == NULL) {
            free(currentName);
            return NULL;
        }
        currentFile = currentFile->next;
    }
    free(currentName);
    return currentFile->file;
}


//Start of operations ****************************************************



int fuseRead (const char *path, char *buff, size_t size, off_t offset, struct fuse_file_info *fi ){
    printf("read %s\n", path);
    fflush(stdout);
    Files *currentFile = getFile(path);
    if (currentFile == NULL){
        return 1;
    }
    if(currentFile->content == NULL){
        return 0;
    }
    memcpy(buff, currentFile->content + offset, size);
    return 0;
}

int fuseWrite(const char *path, const char *buff, size_t size, off_t offset, struct fuse_file_info *fi) {
    printf("write %s\n", path);
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
    printf("readDir %s\n", path);
    fflush(stdout);
    filler( buff, ".", NULL, 0 ); 	// Current Directory
	filler( buff, "..", NULL, 0 ); 	// Parent Directory



	FileDescriptor *currentFile = getToFile(path)->files;

	if (currentFile == NULL){
        return 0;
	}

    Files *files;
	if(currentFile->directory != NULL) {
        files = currentFile->directory;
    } else{
        return 0;
	}

    while (currentFile != NULL && currentFile->name != NULL){
        filler(buff, currentFile->name, NULL, 0 );
        currentFile = currentFile->next;
    }

    return 0;
}


int fuseMkdir(const char* path, mode_t mode){
    printf("mkdir %s\n", path);
    fflush(stdout);
    Directory *dir = getToFile(path);
    FileDescriptor *currentFD;

    char* name = getName(path);

    if (dir->files == NULL){
        dir->files = malloc(sizeof(FileDescriptor));
        currentFD = dir->files;
    } else{
        currentFD = dir->files;
        while (currentFD->next != NULL){
            currentFD = currentFD->next;
        }
        currentFD->next = malloc(sizeof(FileDescriptor));
        currentFD = currentFD->next;
    }

    currentFD->name = name;
    currentFD->parent = dir;
    currentFD->type = directory;
    currentFD->directory = NULL;
    currentFD->file = NULL;
    currentFD->next = NULL;

    return 0;
}

int fuseGetattr( const char *path, struct stat *stateBuff ) {
    printf("fun getattr %s\n", path);
    fflush(stdout);

    stateBuff->st_uid = getuid();
    stateBuff->st_gid = getgid();



    return 0;
}

int fuseOpen (const char *path, struct fuse_file_info *fi){
    printf("fuseOpen %s\n", path);
    fflush(stdout);
    return 0;
}

int fuseCreate (const char *path, mode_t mode, struct fuse_file_info *fi){
    printf("fuseCreate %s\n", path);
    fflush(stdout);
    Directory *currentDirectory = getToFile(path);
    FileDescriptor *currentFD;

    if (currentDirectory == NULL){
        return 1;
    }
    if (currentDirectory->files == NULL){
        currentDirectory->files = malloc(sizeof(Files));
        currentFD = currentDirectory->files;
    } else{
        currentFD = currentDirectory->files;
        while (currentFD->next != NULL){
            currentFD = currentFD->next;
        }
    }

    Files* currentFile = malloc(sizeof(Files));
    currentFile->attr = NULL;
    currentFile->content = NULL;
    currentFD->file = currentFile;
    currentFD->name = getName(path);
    currentFD->next = NULL;
    currentFile->content = NULL;

    struct stat *attr = malloc(sizeof(struct stat));

    return 0;
}

//End of operations *****************************************************

static struct fuse_operations operations = {
    .getattr	= fuseGetattr,
    .readdir	= fuseReadDir,
    .read		= fuseRead,
	.write		= fuseWrite,
	.mkdir      = fuseMkdir,
	.open       = fuseOpen,
	.create     = fuseCreate,
};

int main( int argc, char *argv[] ) {
    printf("Started\n");
	return fuse_main( argc, argv, &operations, NULL );
}



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
    unsigned int length;
}Files;

typedef struct directory{
    struct fileDescriptor *files;
    struct fileDescriptor *parent;
} Directory;

typedef struct fileDescriptor{
    FileType type;
    char* name;
    Files *file;
    Directory *directory;
    struct fileDescriptor *next;
    Directory *parent;
}FileDescriptor;


FileDescriptor root;
struct stat defaultStat;


Directory *getToFile(const char* path){
    FileDescriptor *currentFD = root.directory->files;
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

FileDescriptor *getFD(const char* path){
    FileDescriptor *currentFD = getToFile(path)->files;
    if (strcmp(path, "/") == 0){
        return &root;
    }
    if(currentFD == NULL){
        return NULL;
    }

    char *currentName = getName(path);
    while (strcmp(currentFD->name, currentName) != 0){
        if (currentFD->next == NULL) {
            free(currentName);
            return NULL;
        }
        currentFD = currentFD->next;
    }

    free(currentName);
    return currentFD;
}


//Start of operations ****************************************************



int fuseRead (const char *path, char *buff, size_t size, off_t offset, struct fuse_file_info *fi ){
    printf("read %s\n", path);
    fflush(stdout);
    Files *currentFile = getFD(path)->file;
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
    Files *file = getFD(path)->file;
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

	FileDescriptor *currentFile = getFD(path);

	if (currentFile == NULL){
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

    stateBuff->st_atime = stateBuff->st_mtime = stateBuff->st_ctime = time(NULL);
    stateBuff->st_uid = getuid();
    stateBuff->st_gid = getgid();

    FileDescriptor *fd = getFD(path);
    if (fd == NULL){
        stateBuff->st_nlink = 0;
        stateBuff->st_mode = 777;
        return 0;
    }



    if (fd->type == directory){
        stateBuff->st_mode = S_IFDIR | 0755;
        stateBuff->st_nlink = 2;
    } else if(fd->type == file){
        stateBuff->st_mode = S_IFREG | 0644;
        stateBuff->st_nlink = 1;
        stateBuff->st_size = fd->file->length;
    }

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
    currentFile->content = NULL;
    currentFD->file = currentFile;
    currentFD->name = getName(path);
    currentFD->type = file;
    currentFD->next = NULL;
    currentFile->content = NULL;


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
    root.directory = malloc(sizeof(Directory));
    root.directory->files = NULL;
    root.directory->parent = &root;
    root.type = directory;
    mode_t mode;
    fuseCreate("/bla", mode, malloc(sizeof(struct fuse_file_info)));

    int ret = fuse_main( argc, argv, &operations, NULL );

    sleep(30);

	return ret;
}

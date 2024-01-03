#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MAX_PATH_LENGTH 256
#define MAX_FILES 256

// Structure to store file statistics
typedef struct {
    char filename[MAX_PATH_LENGTH];
    off_t size;
} FileInfo;

// Structure to store file statistics
typedef struct {
    char path[MAX_PATH_LENGTH];
    struct data* data;
} FolderInfo;

typedef struct {
    char extension[10];  // adjust the size based on your needs
    int count;
}FileType ;

// Structure to store data (thread or process)
struct data {
    char path[MAX_PATH_LENGTH];
    FileInfo file_infos[MAX_FILES];
    FileInfo largest_file;
    FileInfo smallest_file;
    long total_files;       // Added member for total number of files
    int num_file_types;    // Added member for number of types of files
    long total_folders;     // Added member for total number of folders
    off_t final_size;       // Added member for final size of the root folder
    int smallest_file_set;  // Flag to indicate if smallest_file.size has been set
    pthread_mutex_t mutex;  // Mutex for synchronization
    FileType fileTypes[MAX_FILES];
};

void analyzeFolder(const char *dirPath, struct data *shared_data);
void analyzeFile(const char *path, struct data *shared_data);

// Thread function to calculate file size
void *threadAnalyze(void *arg) {
    FolderInfo *folderInfo = (FolderInfo *)arg;
    pthread_t tid = pthread_self();
    printf("Thread %lu is analyzing folder: %s\n", (unsigned long)tid, folderInfo->path);
    printf("********************\n");
    analyzeFolder(folderInfo->path, folderInfo->data);
    return NULL;
}
char *getType(const char *filePath){
    char *splitName = strrchr(filePath, '.');
    if(!splitName || splitName == filePath)
        return "";
    return splitName + 1;
}
void addType(FileType* fType, char* type, int* len){
    for(int i = 0; i  < (*len); i++){
        if(strcmp(fType[i].extension, type) == 0){
            fType[i].count++;
            //printf("test\n");
            return;
        }
    }
    FileType ft;
    ft.count = 1;
    strcpy(ft.extension, type);
    fType[*len] = ft;
    (*len)++;
}
void analyzeFolder(const char *dirPath, struct data *shared_data) {
    DIR *dir;
    struct dirent *entry;
    // Open directory
    dir = opendir(dirPath);
    if (!dir) {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }

    pthread_t threads[100]; // Assuming a maximum of 1024 threads
    FolderInfo *folderInfos = malloc(1024 * sizeof(FolderInfo)); // Allocate memory dynamically
    if (!folderInfos) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    int numFolder = 0;
    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Ignore current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        // Construct the full path of the file
        snprintf(folderInfos[numFolder].path, sizeof(folderInfos[numFolder].path), "%s/%s", dirPath, entry->d_name);
        folderInfos[numFolder].data = shared_data;
        // Check if the entry is a directory
        if (entry->d_type == DT_DIR) {
            // This is a subdirectory, analyze it with threads
            //printf("path %s\n", folderInfos[numFolder].path);
            if (pthread_create(&threads[numFolder], NULL, threadAnalyze, &folderInfos[numFolder]) != 0) {
                perror("Error creating thread");
                exit(EXIT_FAILURE);
            }
            numFolder++;
        } else {
            // Check if it's a regular file
            analyzeFile(folderInfos[numFolder].path, shared_data);
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < numFolder; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("Error joining thread");
            exit(EXIT_FAILURE);
        }
    }
    free(folderInfos);
}
void analyzeFile(const char *path, struct data *shared_data) {
    struct stat file_stat;
    char file_path[MAX_PATH_LENGTH];
    snprintf(file_path, sizeof(file_path), "%s", path);

    // Check if the entry is a regular file
    if (stat(file_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
        // Update total number of files
        // Lock the mutex before accessing shared data
        pthread_mutex_lock(&shared_data->mutex);
        if (shared_data->total_files < MAX_FILES) {
            // Copy file info to the array
            FileInfo temp;
            temp.size = file_stat.st_size;
            strcpy(temp.filename, file_path);
            memcpy(&shared_data->file_infos[shared_data->total_files], &temp, sizeof(FileInfo));
        }
        // Update total number of files in the shared data
        shared_data->total_files++;
        shared_data->final_size += file_stat.st_size;
        // Update largest_file and smallest_file in the shared data
        if (file_stat.st_size > shared_data->largest_file.size) {
            shared_data->largest_file.size = file_stat.st_size;
            strcpy(shared_data->largest_file.filename, file_path);
        }
        if (!shared_data->smallest_file_set || file_stat.st_size < shared_data->smallest_file.size) {
            shared_data->smallest_file.size = file_stat.st_size;
            strcpy(shared_data->smallest_file.filename, file_path);
            shared_data->smallest_file_set = 1;  // Set the flag
        }
        addType(shared_data->fileTypes, getType(file_path), &shared_data->num_file_types);
        // Unlock the mutex after updating shared data
        pthread_mutex_unlock(&shared_data->mutex);
    }
}

void firstDepth(const char *path, struct data *shared_data) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;

    // Open the directory
    dir = opendir(path);

    // Check for errors
    if (dir == NULL) {
        fprintf(stderr, "Unable to open directory %s\n", path);
        exit(EXIT_FAILURE);
    }
    // Count the folders in the directory
    while ((entry = readdir(dir)) != NULL) {
        char file_path[MAX_PATH_LENGTH];
        snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
        // Check if the entry is a directory and not "." or ".."
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            // This is a directory, fork a new process
            pid_t childProcessID = fork();
            if (childProcessID < 0) {
                perror("Fork error");
                exit(EXIT_FAILURE);
            } else if (childProcessID == 0) {
                // Child process
                char child_path[512];
                snprintf(child_path, sizeof(child_path), "%s/%s", path, entry->d_name);
                printf("Process %d is analyzing directory: %s\n", getpid(), child_path);
                printf("----------------------\n");
                pthread_mutex_lock(&shared_data->mutex);
                shared_data->total_folders++;
                pthread_mutex_unlock(&shared_data->mutex);
                analyzeFolder(child_path, shared_data);
                // Close the directory and exit
                closedir(dir);
                exit(EXIT_SUCCESS);
            }
        }
            // Check if it's a regular file
        else if (stat(file_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
            analyzeFile(file_path, shared_data);
        }
    }
    while(wait(NULL) != -1);
    // Close the directory
    closedir(dir);
}
void printResult(struct data *shared_data){
    printf("Total number of files: %ld\n", shared_data->total_files);
    printf("Number of types of files: %d\n", shared_data->num_file_types);
    printf("Largest file: %s, Size: %ld bytes\n", shared_data->largest_file.filename, shared_data->largest_file.size);
    printf("Smallest file: %s, Size: %ld bytes\n", shared_data->smallest_file.filename, shared_data->smallest_file.size);
    printf("Final size of the root folder: %ld bytes\n", shared_data->final_size);
    for (int i = 0; i < shared_data->total_files; ++i) {
        printf("file %d: size: %ld bytes path: %s\n", i+1, shared_data->file_infos[i].size, shared_data->file_infos[i].filename);
    }
    for (int i = 0; i < shared_data->num_file_types; ++i) {
        printf("%s: %d\n", shared_data->fileTypes[i].extension, shared_data->fileTypes[i].count);
    }
}
int main(int argc, char *argv[]){
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    // Initialize shared memory using mmap
    int shared_memory_fd = shm_open("/shared_data", O_CREAT | O_RDWR, S_IRWXU);
    ftruncate(shared_memory_fd, sizeof(struct data));
    struct data *shared_data = mmap(NULL, sizeof(struct data), PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_fd, 0);

    strcpy(shared_data->path, argv[1]);
    shared_data->total_folders = 0;  // Initialize total_folders to 0
    pthread_mutex_init(&shared_data->mutex, NULL);

    // Call the function to count folders and spawn threads for file analysis
    firstDepth(shared_data->path, shared_data);

    // Display results
    printResult(shared_data);
    // Cleanup: close shared memory and unlink
    munmap(shared_data, sizeof(struct data));
    close(shared_memory_fd);
    shm_unlink("/shared_data");
    pthread_mutex_destroy(&shared_data->mutex);
}
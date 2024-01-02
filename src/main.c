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
#define MAX_PROCESSES 100

// Structure to store file statistics
typedef struct {
    char filename[256];
    off_t size;
} FileInfo;

// Structure to store data (thread or process)
struct data {
    char path[MAX_PATH_LENGTH];
    FileInfo largest_file;
    FileInfo smallest_file;
    long total_files;       // Added member for total number of files
    long num_file_types;    // Added member for number of types of files
    long total_folders;     // Added member for total number of folders
    off_t final_size;       // Added member for final size of the root folder
    int smallest_file_set;  // Flag to indicate if smallest_file.size has been set
    pthread_mutex_t mutex;  // Mutex for synchronization
};
void nDepth(const char *path, struct data *shared_data){
    
}
void firstDepth(const char *path, struct data *shared_data) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    int folderCount = 0;

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
                pthread_mutex_lock(&shared_data->mutex);
                shared_data->total_folders++;
                pthread_mutex_unlock(&shared_data->mutex);
                
//                pthread_t thread;
//                struct data *subfolder_data = malloc(sizeof(struct data));
//                snprintf(subfolder_data->path, sizeof(subfolder_data->path), "%s/%s", data->path, entry->d_name);
//                pthread_create(&thread, NULL, thread_function, subfolder_data);
//                pthread_join(thread, NULL);
//                free(subfolder_data);
                
                // Close the directory and exit
                closedir(dir);
                break;
            } else {
                // Parent process
                // Wait for the child process to finish
                waitpid(childProcessID, NULL, 0);
            }
        }
        // Check if it's a regular file
        else if (stat(file_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
            // Update total number of files
            // Lock the mutex before accessing shared data
            pthread_mutex_lock(&shared_data->mutex);

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

            // Unlock the mutex after updating shared data
            pthread_mutex_unlock(&shared_data->mutex);
        }
    }
    // Close the directory
    closedir(dir);
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
    printf("Total number of files: %ld\n", shared_data->total_files);
    printf("Number of types of files: %ld\n", shared_data->num_file_types);
    printf("Largest file: %s, Size: %ld bytes\n", shared_data->largest_file.filename, shared_data->largest_file.size);
    printf("Smallest file: %s, Size: %ld bytes\n", shared_data->smallest_file.filename, shared_data->smallest_file.size);
    printf("Final size of the root folder: %ld bytes\n", shared_data->final_size);
    // Cleanup: close shared memory and unlink
    munmap(shared_data, sizeof(struct data));
    close(shared_memory_fd);
    shm_unlink("/shared_data");
    pthread_mutex_destroy(&shared_data->mutex);


}
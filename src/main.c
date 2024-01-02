#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>

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
};
int countFolders(const char *path) {
    DIR *dir;
    struct dirent *entry;
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

                // Enqueue the child process ID
                //enqueue(processQueue, getpid());

                // Close the directory and exit
                closedir(dir);
                break;
            } else {
                // Parent process
                // Wait for the child process to finish
                waitpid(childProcessID, NULL, 0);
            }
        }
    }
    // Close the directory
    closedir(dir);
    return folderCount;
}
int main(int argc, char *argv[]){
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    struct data main_data;
    strcpy(main_data.path, argv[1]);
    main_data.total_folders = countFolders(main_data.path);

}

# **Semi-NCDU**
This project is a simulation of NCDU as our midterm project. In this project we give a directory as the input; In the main folder we generate a child process with fork() function for each folder. After the first leyer(in each main sub folder) we create a thread for each subfolder. In this way the thread of folder that consists files, will analyse them.
# *How to use*

For running this project you should go to the directory of your executable file, open a terminal and enter this command:

```bash
  main.exe <path>
```
## *Functions Explaination*

#### *analyzeFile*
This function analyzes files and will assign the total size, smallest size and the largest size of files.
#### *analyzeFoder*
This function analyzes folders. It checks if there is a subfoder, it creates a thread for it and if there is a file it calls analyzeFile.
#### *ThreadAnalyze*
This function creates thread for each folder then it calls analyzeFoder.
#### *firstDepth*
This function generates child with fork() function for each folder in the first layer(the main folder) and for each file it calls analyzeFile.

### *Producesrs*

 *Melika Hassanzade Eskafi*

 *Fateme Dhbashi*
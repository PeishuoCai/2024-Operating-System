#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
int main() {

    int pid = fork();
    if (pid == 0) {
        printf("I am the child\n");
        sleep(60);
    } else {
        printf("%d\n", pid);
        printf("I am the parent\n");
        // char *dir = "/proc/%d/fd", pid;
        char proc_path[256];  // Array to store the path
        sprintf(proc_path, "/proc/%d/fd", pid);  // Format the string with child PID

        printf("Proc path: %s\n", proc_path);
        DIR *dirt;
        struct dirent *entry;
        dirt = opendir(proc_path);
        entry = readdir(dirt);
        while( (entry=readdir(dirt)) )
        {
            if (entry->d_type == 10 && strcmp(entry->d_name, "0") != 0 && strcmp(entry->d_name, "1") != 0 && strcmp(entry->d_name, "2") != 0){
                int number = atoi(entry->d_name);
                printf("%d\n", number);
                close(number);
            }

        }
        printf("Parent done\n");
        while( (entry=readdir(dirt)) )
        {
            // printf("%d\n", entry->d_type);
            printf("%s\n", entry->d_name);
            int number = atoi(entry->d_name);
            // printf("%d\n",strcmp(entry->d_name,"1"));
            // if (strcmp(entry->d_name,"1") == 0) {
            //     printf("GAY %s", entry->d_name);
            // }
            printf("%d\n", number);

        }
    }
    return 0;
}
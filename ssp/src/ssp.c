#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    int ssp_id;
    int status;
    int pid;
    char name[50];
}ssp_list;

ssp_list *processes;
int size;
ssp_list *reaped_exits;
int reap_size;

void check_error(int ret, const char *msg) {
    if (ret == -1) {
        int err = errno;
        perror(msg);
        exit(err);
    }
}



void reap_children() {
    while (1) {
        int status;
        int pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0) {
            // printf("%d\n", pid);
            // printf("No more children to reap\n");
            break;
        }
        else{
            // printf("Reaped %d\n", pid);
            int found = 0; // 0 means not found
            for (int i = 0; i < size-1; i++) {
                if (processes[i].pid == pid) {
                    if (WIFSIGNALED(status)) {
                        processes[i].status = WTERMSIG(status) + 128;
                    }
                    else if (WIFEXITED(status)) {
                        processes[i].status = WEXITSTATUS(status);
                    }
                    else {
                        int err = errno;
                        perror("waitpid");
                        exit(err);
                    }
                    // printf("Reaped orig child:%d\n", pid);
                    found = 1;
                    break;
                }

            }
            if (found == 0) {
                reap_size = reap_size + 1;
                ssp_list *new_reaped_exits = realloc(reaped_exits, reap_size * sizeof(ssp_list));
                reaped_exits = new_reaped_exits;
                if (reaped_exits == NULL) {
                    int err = errno;
                    perror("list creation");
                    exit(err);
                }

                reaped_exits[reap_size-1].pid = pid;
                if (WIFSIGNALED(status)) {
                    reaped_exits[reap_size-1].status = WTERMSIG(status) + 128;
                }
                else if (WIFEXITED(status)) {
                    reaped_exits[reap_size-1].status = WEXITSTATUS(status);
                }
                else {
                    int err = errno;
                    perror("waitpid");
                    exit(err);
                }
                reaped_exits[reap_size-1].ssp_id = reap_size-1;
                strncpy(reaped_exits[reap_size-1].name, "<unknown>", sizeof(reaped_exits[reap_size-1].name) - 1);
                reaped_exits[reap_size-1].name[sizeof(reaped_exits[reap_size-1].name) - 1] = '\0';
            }
        }
    }
}

void ssp_init() {
    struct sigaction sa;
    sa.sa_handler = reap_children;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
    size = 1;
    processes = malloc(size * sizeof(ssp_list));
    reap_size = 0;
    reaped_exits = malloc(reap_size * sizeof(ssp_list));

    if (processes == NULL) {
        int err = errno;
        perror("list creation");
        exit(err);
    }
    prctl(PR_SET_CHILD_SUBREAPER, 1);
    // reap_children();
    

    
}

int ssp_create(char *const *argv, int fd0, int fd1, int fd2) {
    
    int pid = fork();
    check_error(pid, "fork");
    
    if (pid == 0) {
         
        pid = getpid();
        check_error(dup2(fd0, 0), "dup2");
        // check_error(close(fd0), "close fd");

        check_error(dup2(fd1, 1), "dup2");
        // check_error(close(fd1), "close fd");

        check_error(dup2(fd2, 2), "dup2");
        // check_error(close(fd2), "close fd");

        

        char proc_path[256];  // Array to store the path
        sprintf(proc_path, "/proc/%d/fd", pid);  // Format the string with child PID

        DIR *dirt;
        struct dirent *entry;
        dirt = opendir(proc_path);
        if (dirt == NULL) {
            int err = errno;
            perror("opendir");
            exit(err);
        }
        entry = readdir(dirt);
        if (entry == NULL) {
            int err = errno;
            perror("readdir");
            exit(err);
        }
        while( (entry=readdir(dirt)) )
        {
            if (entry->d_type == 10 && strcmp(entry->d_name, "0") != 0 && strcmp(entry->d_name, "1") != 0 && strcmp(entry->d_name, "2") != 0){
                int number = atoi(entry->d_name);
                check_error(close(number), "close fd");
            }

        }
        closedir(dirt);


        check_error(execvp(argv[0], argv), "execvp");
    }
    else {
        size = size + 1;
        ssp_list *new_processes = realloc(processes, size * sizeof(ssp_list));
        processes = new_processes;
        if (processes == NULL) {
            int err = errno;
            perror("list creation");
            exit(err);
        }
        char name[strlen(argv[0]) + 1];
        strcpy(name, argv[0]);
        strncpy(processes[size-2].name, name, sizeof(processes[size-2].name) - 1);
        processes[size-2].name[sizeof(processes[size-2].name) - 1] = '\0';


        processes[size-2].pid = pid;
        processes[size-2].status = -1;
        processes[size-2].ssp_id = size-2;
        // printf("Added process: PID = %d, Name = %s  leng=%d ", processes[size - 1].pid, processes[size - 1].name, strlen(processes[size - 1].name));
    }
    return processes[size-2].ssp_id;
}

int ssp_get_status(int ssp_id) {
    int pid = processes[ssp_id].pid;
    int status;
    if (processes[ssp_id].status == -1) {
        pid_t result = waitpid(pid, &status, WNOHANG);
        check_error(result, "waitpid");
        // printf("status %d\n", status);


        if (result == 0) {
            processes[ssp_id].status = -1;
            return -1;
        }

        else if (WIFSIGNALED(status)) {
            // printf("%s terminated by signal %d\n", processes[ssp_id].name, WTERMSIG(status));
            processes[ssp_id].status = WTERMSIG(status) + 128;
            return WTERMSIG(status) + 128;
        }
        else if (WIFEXITED(status)) {
            // printf("%s exited with status %d\n", processes[ssp_id].name, WEXITSTATUS(status));
            processes[ssp_id].status = WEXITSTATUS(status);
            return WEXITSTATUS(status);
        }

        
        else {
            int err = errno;
            perror("waitpid");
            exit(err);
        }
    }
    else {
        return processes[ssp_id].status;
    }
}

void ssp_send_signal(int ssp_id, int signum) {
    int pid = processes[ssp_id].pid;
    if (kill(pid, 0) == 0) {
        if (kill(pid, signum) == -1) {
            int err = errno;
            perror("send signal");
            exit(err);
        }
        int status = ssp_get_status(ssp_id);
    }
    
}

void ssp_wait() {
    for (int i = 0; i < size-1; i++) {
        int status;
        if (processes[i].status == -1) {
            check_error(waitpid(processes[i].pid, &status, 0), "waitpid");
            if (WIFSIGNALED(status)) {
                processes[i].status = WTERMSIG(status) + 128;
            }
            else if (WIFEXITED(status)) {
                processes[i].status = WEXITSTATUS(status);
            }
            
        }
        if (processes[i].status < 0 || processes[i].status > 255) {
            perror("not exited");
            exit(-1);
        }
    }
}

void ssp_print() {

    int max_length = 3;
    for (int j=0; j < size-1; j++) {
        // printf("%s %d",processes[j].name, processes[j].ssp_id);
        if (strlen(processes[j].name) > max_length) {
            max_length = strlen(processes[j].name);
            // printf("%d  %d",processes[j].pid, max_length);
        }
    }
    if (reap_size > 0) {
        for (int j=0; j < reap_size; j++) {
            if (strlen(reaped_exits[j].name) > max_length) {
                max_length = strlen(reaped_exits[j].name);
            }
        }
    }
    const char *str1 = "PID";
    const char *str2 = "CMD";
    const char *str3 = "STATUS";
    printf("%7s %-*s %s\n", str1, max_length, str2, str3);
    for (int i=0; i < size-1; i++) {
        ssp_get_status(i);
        printf("%7d %-*s %d\n", processes[i].pid, max_length, processes[i].name, processes[i].status);
    }

    if (reap_size > 0) {
        for (int i=0; i < reap_size; i++) {
            printf("%7d %-*s %d\n", reaped_exits[i].pid, max_length, reaped_exits[i].name, reaped_exits[i].status);
        }
    }
}



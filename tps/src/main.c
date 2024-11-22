#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

int is_numerical(const char *str) {
    int i = 0;
    while(str[i] != '\0') {
        if(!isdigit(str[i])) {
            return 0;
        }
        i++;
    }
    return 1;
}

int find_file(const char *path, const char *filename) {
    struct dirent *entry;
    DIR *dir = opendir(path);

    if (dir == NULL) {
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry -> d_name, ".") == 0 || strcmp(entry -> d_name, "..") == 0) {
            continue;
        }
        if (strcmp(entry -> d_name, filename) == 0) {
            closedir(dir);
            return 1;
        }
    }
    closedir(dir);
    return 0;
}

char *read_for_cmd(const char *path) {
    FILE *file = fopen(path, "r");
    char *line = NULL;
    size_t len = 0;

    if (file == NULL) {
        return NULL;
    }
    while (getline(&line, &len, file) != -1) {
        if (strncmp(line, "Name:", 5) == 0) {
            fclose(file);
            char *result = line + 5;
            while (isspace(*result)) {
                result++;
            }
            result = strdup(result);
            free(line);
            return result;
        }
    }

    fclose(file);
    free(line);
    return NULL;
}

int main() {
    DIR *dir;
    struct dirent *entry;
    const char *str1 = "PID";
    
    printf("%5s CMD\n", str1);
    dir = opendir("/proc");
    entry = readdir(dir);
    while( (entry=readdir(dir)) )
    {
        if (is_numerical(entry->d_name)) {
            char str[256];
            snprintf(str, sizeof(str), "/proc/%s", entry->d_name);
            if (find_file(str, "status")) {
                strcat(str, "/status");
                char *result = read_for_cmd(str);
                printf("%5s %s", entry->d_name, result);
                free(result);
            }
        }
    }
    closedir(dir);
    return 0;
}

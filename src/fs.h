#ifndef FS_H
#define FS_H

#include <stdint.h>

#define MAX_FILES 32
#define MAX_FILENAME 64
#define MAX_FILE_SIZE 4096

typedef struct {
    char names[MAX_FILES][MAX_FILENAME];
    int count;
} dir_list_t;

int fs_init(void *SystemTable);
int fs_list_root(dir_list_t *list);
int fs_read(const char *filename, char *buf, int max_len);
int fs_write(const char *filename, const char *buf, int len);

#endif

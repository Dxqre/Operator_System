/**
 * @file    simplefs.c
 * @brief   Definition in FAT16 file system.
 * @details Macro definitions, structs such as FCB and FAT, and some global variable.
 * @author  Leslie Van
 * @date    2018-12-19 to
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "simplefs.h"


/* Definition of functions */
/**
 * Start file system and initial variable.
 * @author Leslie Van
 */
int start_sys(void) {
    fs_head = (unsigned char *) malloc(DISK_SIZE);
    memset(fs_head, 0, DISK_SIZE);
    FILE *fp;
    int i;

    if ((fp = fopen(SYS_PATH, "r")) != NULL) {
        fread(fs_head, DISK_SIZE, 1, fp);
        fclose(fp);
    } else {
        printf("System is not initialized, now install it and create system file.\n");
        printf("Please don't leave program.\n");
        printf("Initialed success!\n");
        do_format();
    }

    /**< Init the first openfile entry. */
    fcb_cpy(&openfile_list[0].open_fcb, ((fcb *) (fs_head + 5 * BLOCK_SIZE)));
    strcpy(openfile_list[0].dir, ROOT);
    openfile_list[0].count = 0;
    openfile_list[0].fcb_state = 0;
    openfile_list[0].free = 1;
    curdir = 0;

    if (find_fcb("/tmp.txt") != NULL) {
        fcb_cpy(&openfile_list[1].open_fcb, find_fcb("/tmp.txt"));
        strcpy(openfile_list[1].dir, ROOT);
        openfile_list[1].count = 0;
        openfile_list[1].fcb_state = 0;
        openfile_list[1].free = 1;
    }

    if (find_fcb("/fold") != NULL) {
        fcb_cpy(&openfile_list[2].open_fcb, find_fcb("/fold"));
        strcpy(openfile_list[2].dir, "/fold");
        openfile_list[2].count = 0;
        openfile_list[2].fcb_state = 0;
        openfile_list[2].free = 1;
    }

    /**< Init the other openfile entry. */
    fcb *empty = (fcb *) malloc(sizeof(fcb));
    set_fcb(empty, "\0", "\0", 0, 0, 0, 0);
    for (i = 3; i < MAX_OPENFILE; i++) {
        fcb_cpy(&openfile_list[i].open_fcb, empty);
        strcpy(openfile_list[i].dir, "\0");
        openfile_list[i].free = 0;
        openfile_list[i].count = 0;
        openfile_list[i].fcb_state = 0;
    }

    /**< Init global variables. */
    strcpy(current_dir, openfile_list[curdir].dir);
    start = ((block0 *) fs_head)->start_block;
    free(empty);
    return 0;
}

/**
 * Entry for command "format".
 * @author Leslie Van
 */
int my_format(char **args) {
    unsigned char *ptr;
    FILE *fp;
    int i;

    /**< Check argument count. */
    for (i = 0; args[i] != NULL; i++);
    if (i > 2) {
        fprintf(stderr, "format: expected argument to \"format\"\n");
        return 1;
    }

    /**< Check argument value. */
    if (args[1] != NULL) {
        /**< Fill with 0. */
        if (!strcmp(args[1], "-x")) {
            ptr = (unsigned char *) malloc(DISK_SIZE);
            memset(ptr, 0, DISK_SIZE);
            fp = fopen(SYS_PATH, "w");
            fwrite(ptr, DISK_SIZE, 1, fp);
            free(ptr);
            fclose(fp);
        } else {
            fprintf(stderr, "format: expected argument to \"format\"\n");
            return 1;
        }
    }
    do_format();

    return 1;
}

/**
 * Fast format file system.
 * Create boot block, file allocation tables and root directory.
 * @author Leslie Van
 */
int do_format(void) {
    unsigned char *ptr = fs_head;
    int i;
    int first, second;
    FILE *fp;

    /**< Init the boot block(block0). */
    block0 *init_block = (block0 *) ptr;
    strcpy(init_block->information,
           "Disk Size = 1MB, Block Size = 1KB, Block0 in 0, FAT0/1 in 1/3, Root Directory in 5");
    init_block->root = 5;
    init_block->start_block = init_block + BLOCK_SIZE * 7;
    ptr += BLOCK_SIZE;

    /**< Init FAT0/1. */
    set_free(0, 0, 2);

    /**< Allocate 5 blocks to one block0(1) and two fat(2). */
    set_free(get_free(1), 1, 0);
    set_free(get_free(2), 2, 0);
    set_free(get_free(2), 2, 0);

    ptr += BLOCK_SIZE * 4;

    /**< 2 blocks to root directory. */
    fcb *root = (fcb *) ptr;
    first = get_free(ROOT_BLOCK_NUM);
    set_free(first, ROOT_BLOCK_NUM, 0);
    set_fcb(root, ".", "di", 0, first, BLOCK_SIZE * 2, 1);
    root++;
    set_fcb(root, "..", "di", 0, first, BLOCK_SIZE * 2, 1);
    root++;

    /**< Create test file and test folder.(delete after test) */
    second = get_free(1);
    set_free(second, 1, 0);
    set_fcb(root, "tmp", "txt", 1, second, 16, 1);
    root++;
    second = get_free(1);
    set_free(second, 1, 0);
    set_fcb(root, "fold", "di", 0, second, BLOCK_SIZE, 1);
    init_folder(first, second);
    root++;

    // for (i = 2; i < BLOCK_SIZE * 2 / sizeof(fcb); i++, root++) {
    for (i = 4; i < BLOCK_SIZE * 2 / sizeof(fcb); i++, root++) {
        root->free = 0;
    }

    memset(fs_head + BLOCK_SIZE * 7, 'a', 15);
    /**< Write back. */
    fp = fopen(SYS_PATH, "w");
    fwrite(fs_head, DISK_SIZE, 1, fp);
    fclose(fp);
}

/**
 *
 * @author
 * @param args
 */
int my_cd(char **args) {
    int i;
    int fd;
    fcb *dir;

    /**< Check argument count. */
    for (i = 0; args[i] != NULL; i++);
    if (i != 2) {
        fprintf(stderr, "cd: expected argument to \"format\"\n");
        return 1;
    }

    /**< Check argument value. */
    dir = find_fcb(args[1]);
    if (dir == NULL || dir->attribute == 1) {
        fprintf(stderr, "cd: No such folder\n");
        return 1;
    }

    /**< Check if the folder fcb exist in openfile_list. */
    for (i = 0; i < 10; i++) {
        if (!strcmp(dir->filename, openfile_list[i].open_fcb.filename) &&
            dir->first == openfile_list[i].open_fcb.first) {
            /**< Folder is open. */
            do_chdir(i);
            return 1;
        }
    }

    /**< @todo With do_open() implement, check and change it here. */
    /**< Folder is close, open it and change current directory. */
    if ((fd = do_open(args[1])) > 0) {
        do_chdir(fd);
    }

    return 1;
}

void do_chdir(int fd) {
    curdir = fd;
    memset(current_dir, '\0', sizeof(current_dir));
    strcpy(current_dir, openfile_list[curdir].dir);
}

int my_pwd(char **args) {
    /**< Check argument count. */
    if (args[1] != NULL) {
        fprintf(stderr, "cd: too many arguments\n");
        return 1;
    }

    printf("%s\n", current_dir);
    return 1;
}

/**
 *
 * @author
 * @param args
 */
int my_mkdir(char **args) {
    int i;
    char path[PATHLENGTH];
    char parpath[PATHLENGTH], dirname[NAMELENGTH];
    char *end;

    /**< Check argument count. */
    if (args[1] == NULL) {
        fprintf(stderr, "mkdir: missing operand\n");
        return 1;
    }

    /**< Do mkdir. */
    for (i = 1; args[i] != NULL; i++) {
        /**< Split path into parent folder and current child folder. */
        get_abspath(path, args[i]);
        end = strrchr(path, '/');
        if (end == path) {
            strcpy(parpath, "/");
            strcpy(dirname, path + 1);
        } else {
            strncpy(parpath, path, end - path);
            strcpy(dirname, end + 1);
        }

        if (find_fcb(parpath) == NULL) {
            fprintf(stderr, "create: cannot create \'%s\': Parent folder not exists\n", parpath);
            continue;
        }
        if (find_fcb(path) != NULL) {
            fprintf(stderr, "create: cannot create \'%s\': Folder or file exists\n", args[i]);
        }

        do_mkdir(parpath, dirname);
    }

    return 1;
}

/**
 *
 * @return
 */
int do_mkdir(const char *parpath, const char *dirname) {
    int second = get_free(1);
    int i, flag = 0, first = find_fcb(parpath)->first;
    fcb *dir = (fcb *) (fs_head + BLOCK_SIZE * first);

    /**< Check for free fcb. */
    for (i = 0; i < BLOCK_SIZE / sizeof(fcb); i++, dir++) {
        if (dir->free == 0) {
            flag = 1;
            break;
        }
    }
    if (!flag) {
        fprintf(stderr, "mkdir: Cannot create more file in %s\n", parpath);
        return -1;
    }

    /**< Check for free space. */
    if (second == -1) {
        fprintf(stderr, "mkdir: No more space\n");
        return -1;
    }
    set_free(second, 1, 0);

    /**< Set fcb and init folder. */
    set_fcb(dir, dirname, "di", 0, second, BLOCK_SIZE, 1);
    init_folder(first, second);

    return 0;
}

/**
 *
 * @todo Implement for my_rmdir().
 * @author
 * @param args
 */
int my_rmdir(char **args) {
    int i, j;
    fcb *dir;

    /**< Check argument count. */
    if (args[1] == NULL) {
        fprintf(stderr, "rmdir: missing operand\n");
        return 1;
    }

    /**< Do remove. */
    for (i = 1; args[i] != NULL; i++) {
        if (!strcmp(args[i], ".") || !strcmp(args[i], "..")) {
            fprintf(stderr, "rmdir: cannot remove %s: '.' or '..' is read only \n", args[i]);
            return 1;
        }

        if (!strcmp(args[i], "/")) {
            fprintf(stderr, "rmdir:  Permission denied\n", args[i]);
            return 1;
        }

        dir = find_fcb(args[i]);
        if (dir == NULL) {
            fprintf(stderr, "rmdir: cannot remove %s: No such folder\n", args[i]);
            return 1;
        }

        if (dir->attribute == 1) {
            fprintf(stderr, "rmdir: cannot remove %s: Is a directory\n", args[i]);
            return 1;
        }

        /**< Check if the folder fcb exist in openfile_list. */
        for (j = 0; j < 10; j++) {
            if (openfile_list[j].free == 0) {
                continue;
            }

            if (!strcmp(dir->filename, openfile_list[j].open_fcb.filename) &&
                dir->first == openfile_list[j].open_fcb.first) {
                /**< Folder is open. */
                fprintf(stderr, "rmdir: cannot remove %s: File is open\n", args[i]);
                return 1;
            }
        }

        do_rmdir(dir);
    }
    return 1;
}

/**
 *
 * @todo Implement for do_rmdir().
 * @return
 */
void do_rmdir(fcb *dir) {
    int first = dir->first;

    dir->free = 0;
    dir = (fcb *) (fs_head + BLOCK_SIZE * first);
    dir->free = 0;
    dir++;
    dir->free = 0;
}

/**
 *
 * @author
 */
int my_ls(char **args) {
    int first = openfile_list[curdir].open_fcb.first;
    int i, mode = 'n';
    int flag[3];
    fcb *dir;

    /**< Check argument count. */
    for (i = 0; args[i] != NULL; i++) {
        flag[i] = 0;
    }
    if (i > 3) {
        fprintf(stderr, "ls: expected argument\n");
        return 1;
    }

    flag[0] = 1;
    for (i = 1; args[i] != NULL; i++) {
        if (args[i][0] == '-') {
            flag[i] = 1;
            if (!strcmp(args[i], "-l")) {
                mode = 'l';
                break;
            } else {
                fprintf(stderr, "ls: wrong operand\n");
                return 1;
            }
        }
    }

    for (i = 1; args[i] != NULL; i++) {
        if (flag[i] == 0) {
            dir = find_fcb(args[i]);
            if (dir != NULL && dir->attribute == 0) {
                first = dir->first;
            } else {
                fprintf(stderr, "ls: cannot access '%s': No such file or directory\n", args[i]);
                return 1;
            }
            break;
        }
    }

    do_ls(first, mode);

    return 1;
}

void do_ls(int first, char mode) {
    int i, count, length = BLOCK_SIZE;
    char fullname[NAMELENGTH], date[16], time[16];
    fcb *root = (fcb *) (fs_head + BLOCK_SIZE * first);
    block0 *init_block = (block0 *) fs_head;
    if (first == init_block->root) {
        length = ROOT_BLOCK_NUM * BLOCK_SIZE;
    }

    if (mode == 'n') {
        for (i = 0, count = 1; i < length / sizeof(fcb); i++, root++) {
            /**< Check if the fcb is used. */
            if (root->free == 0) {
                continue;
            }

            if (root->attribute == 0) {
                printf("%s", FOLDER_COLOR);
                printf("%s\t", root->filename);
                printf("%s", DEFAULT_COLOR);
            } else {
                get_fullname(fullname, root);
                printf("%s\t", fullname);
            }
            if (count % 5 == 0) {
                printf("\n");
            }
            count++;
        }
    } else if (mode == 'l') {
        for (i = 0, count = 1; i < length / sizeof(fcb); i++, root++) {
            /**< Check if the fcb is used. */
            if (root->free == 0) {
                continue;
            }

            trans_date(date, root->date);
            trans_time(time, root->time);
            get_fullname(fullname, root);
            printf("%d\t%6d\t%6ld\t%s\t%s\t", root->attribute, root->first, root->length, date, time);
            if (root->attribute == 0) {
                printf("%s", FOLDER_COLOR);
                printf("%s\n", fullname);
                printf("%s", DEFAULT_COLOR);
            } else {
                printf("%s\n", fullname);
            }
            count++;
        }
    }
    printf("\n");
}

/**
 *
 * @author
 * @param args
 * @return
 * @author
 */
int my_create(char **args) {
    int i;
    char path[PATHLENGTH];
    char parpath[PATHLENGTH], filename[NAMELENGTH];
    char *end;

    /**< Check argument count. */
    if (args[1] == NULL) {
        fprintf(stderr, "create: missing operand\n");
        return 1;
    }

    memset(parpath, '\0', PATHLENGTH);
    memset(filename, '\0', NAMELENGTH);

    /**< Do create */
    for (i = 1; args[i] != NULL; i++) {
        /**< Split parent folder and filename. */
        get_abspath(path, args[i]);
        end = strrchr(path, '/');
        if (end == path) {
            strcpy(parpath, "/");
            strcpy(filename, path + 1);
        } else {
            strncpy(parpath, path, end - path);
            strcpy(filename, end + 1);
        }

        if (find_fcb(parpath) == NULL) {
            fprintf(stderr, "create: cannot create \'%s\': Parent folder not exists\n", parpath);
            continue;
        }
        if (find_fcb(path) != NULL) {
            fprintf(stderr, "create: cannot create \'%s\': Folder or file exists\n", args[i]);
        }

        do_create(parpath, filename);
    }

    return 1;
}

int do_create(const char *parpath, const char *filename) {
    char fullname[NAMELENGTH], fname[16], exname[8];
    char *token;
    int first = get_free(1);
    int i, flag = 0;
    fcb *dir = (fcb *) (fs_head + BLOCK_SIZE * find_fcb(parpath)->first);

    /**< Check for free fcb. */
    for (i = 0; i < BLOCK_SIZE / sizeof(fcb); i++, dir++) {
        if (dir->free == 0) {
            flag = 1;
            break;
        }
    }
    if (!flag) {
        fprintf(stderr, "create: Cannot create more file in %s\n", parpath);
        return -1;
    }

    /**< Check for free space. */
    if (first == -1) {
        fprintf(stderr, "create: No more space\n");
        return -1;
    }
    set_free(first, 1, 0);

    /**< Split name and initial variables. */
    memset(fullname, '\0', NAMELENGTH);
    memset(fname, '\0', 8);
    memset(exname, '\0', 3);
    strcpy(fullname, filename);
    set_free(first, 1, 0);
    token = strtok(fullname, ".");
    strcpy(fname, token);
    token = strtok(NULL, ".");
    strcpy(exname, token);

    /**< Set fcb. */
    set_fcb(dir, fname, exname, 1, first, 0, 1);

    return 0;
}

/**
 *
 * @author
 * @param args
 * @author
 */
int my_rm(char **args) {
    int i, j;
    fcb *file;

    /**< Check argument count. */
    if (args[1] == NULL) {
        fprintf(stderr, "rm: missing operand\n");
        return 1;
    }

    /**< Do remove. */
    for (i = 1; args[i] != NULL; i++) {
        file = find_fcb(args[i]);
        if (file == NULL) {
            fprintf(stderr, "rm: cannot remove %s: No such file\n", args[i]);
            return 1;
        }

        if (file->attribute == 0) {
            fprintf(stderr, "rm: cannot remove %s: Is a directory\n", args[i]);
            return 1;
        }

        /**< Check if the file fcb exist in openfile_list. */
        for (j = 0; j < 10; j++) {
            if (!strcmp(file->filename, openfile_list[i].open_fcb.filename) &&
                file->first == openfile_list[i].open_fcb.first) {
                /**< Folder is open. */
                fprintf(stderr, "rm: cannot remove %s: File is open\n", args[i]);
                return 1;
            }
        }

        do_rm(file);
    }

    return 1;
}


void do_rm(fcb *file) {
    int first = file->first;

    file->free = 0;
    set_free(first, 0, 1);
}

/**
 * @todo Implement for my_open().
 * @param args
 * @return
 */
int my_open(char **args) {
    return 1;
}

/**
 *
 * @todo Implement for do_open().
 * @param filename
 * @return
 * @author
 */
int do_open(char *filename) {

}

/**
 *
 * @todo Implement for my_close().
 * @param args
 * @return
 */
int my_close(char **args) {
    return 1;
}

/**
 *
 * @todo Implement for do_close().
 * @brief
 * @param fd
 * @author
 */
int do_close(int fd) {

}

/**
 *
 * @todo Implement and debug for my_write().
 * @brief
 * @param args
 * @return
 * @author
 */
int my_write(char **args) {
    int i;
    for (i = 0; i < 3; i++) {
        printf("%s\t%d\n", openfile_list[i].open_fcb.filename, openfile_list[i].open_fcb.first);
    }
    do_write(1, 1);
//    fat *fat1 = (fat *) (fs_head + BLOCK_SIZE);



    return 1;
}

/**
 * @todo Debug for do_write().
 * @param fd
 * @param wstyle
 * @return
 */
// int do_write(int fd, char *text, int len, char mode) {
int do_write(int fd, int wstyle) {

}

/**
 *
 * @todo Implement and debug for my_read().
 * @param args
 * @return
 * @author
 */
int my_read(char **args) {
    return 1;
}

/**
 *
 * @todo Debug for do_read().
 * @param fd
 * @param len
 * @param text
 * @return
 * @author
 */
int do_read(int fd, int len, char *text) {

}

/**
 * Exit system, save changes.
 * @author
 */
int my_exit_sys(void) {
    FILE *fp;
    fp = fopen(SYS_PATH, "w");
    fwrite(fs_head, DISK_SIZE, 1, fp);
    free(fs_head);
    fclose(fp);
    return 0;
}

/**
 * Detect free blocks in FAT.
 * @param count Count of needed blocks.
 * @return 0 without enough space, else return the first block number.
 * @author Leslie Van
 */
int get_free(int count) {
    unsigned char *ptr = fs_head;
    fat *fat0 = (fat *) (ptr + BLOCK_SIZE);
    int i, j, flag = 0;
    int fat[1024];

    /** Copy FAT. */
    for (i = 0; i < 1024; i++, fat0++) {
        fat[i] = fat0->id;
    }

    /** Find a continuous space. */
    for (i = 0; i < 1024 - count; i++) {
        for (j = i; j < i + count; j++) {
            if (fat[j] > 0) {
                flag = 1;
                break;
            }
        }
        if (flag) {
            flag = 0;
            i = j;
        } else {
            return i;
        }
    }

    return -1;
}

/**
 * Change value of FAT.
 * @param first The starting block number.
 * @param length The blocks count.
 * @param mode 0 to allocate, 1 to reclaim and 2 to format.
 * @author Leslie Van
 */
int set_free(unsigned short first, unsigned short length, int mode) {
    fat *fat0 = (fat *) (fs_head + BLOCK_SIZE);
    fat *fat1 = (fat *) (fs_head + BLOCK_SIZE * 3);
    int i;
    int offset;

    for (i = 0; i < first; i++, fat0++, fat1++);

    if (mode == 1) {
        /**< Reclaim space. */
        while (fat0->id != END) {
            offset = fat0->id;
            fat0->id = FREE;
            fat1->id = FREE;
            fat0 += offset;
            fat1 += offset;
        }
        fat0->id = FREE;
        fat1->id = FREE;
    } else if (mode == 2) {
        /**< Format FAT */
        for (i = 0; i < BLOCK_NUM; i++, fat0++, fat1++) {
            fat0->id = FREE;
            fat1->id = FREE;
        }
    } else {
        /**< Allocate consecutive space. */
        for (; i < first + length - 1; i++, fat0++, fat1++) {
            fat0->id = first + 1;
            fat1->id = first + 1;
        }
        fat0->id = END;
        fat1->id = END;
    }

    return 0;
}

/**
 * Set fcb attribute.
 * @param f The pointer of fcb.
 * @param filename FCB filename.
 * @param exname FCB file extensions name.
 * @param attr FCB file attribute.
 * @param first FCB starting block number.
 * @param length FCB file length.
 * @param ffree 1 when file occupied, else 0.
 * @author Leslie Van
 */
int set_fcb(fcb *f, const char *filename, const char *exname, unsigned char attr, unsigned short first,
            unsigned long length,
            char ffree) {
    time_t *now = (time_t *) malloc(sizeof(time_t));
    struct tm *timeinfo;
    time(now);
    timeinfo = localtime(now);

    memset(f->filename, 0, 8);
    memset(f->exname, 0, 3);
    strncpy(f->filename, filename, 8);
    strncpy(f->exname, exname, 3);
    f->attribute = attr;
    f->time = get_time(timeinfo);
    f->date = get_date(timeinfo);
    f->first = first;
    f->length = length;
    f->free = ffree;

    free(now);
    return 0;
}

/**
 * Translate ISO time to short time.
 * @param timeinfo Current time structure.
 * @return Time number after translation.
 * @author Leslie Van
 */
unsigned short get_time(struct tm *timeinfo) {
    int hour, min, sec;
    unsigned short result;

    hour = timeinfo->tm_hour;
    min = timeinfo->tm_min;
    sec = timeinfo->tm_sec;
    result = (hour << 11) + (min << 5) + (sec >> 1);

    return result;
}

/**
 * Translate ISO date to short date.
 * @param timeinfo local
 * @return Date number after translation.
 * @author Leslie Van
 */
unsigned short get_date(struct tm *timeinfo) {
    int year, mon, day;
    unsigned short result;

    year = timeinfo->tm_year;
    mon = timeinfo->tm_mon;
    day = timeinfo->tm_mday;
    result = (year << 9) + (mon << 5) + day;

    return result;
}

/**
 * Copy a fcb.
 * @param dest Destination fcb.
 * @param src Source fcb.
 */
int fcb_cpy(fcb *dest, fcb *src) {
    strcpy(dest->filename, src->filename);
    strcpy(dest->exname, src->exname);
    dest->attribute = src->attribute;
    dest->time = src->time;
    dest->date = src->date;
    dest->first = src->first;
    dest->length = src->length;
    dest->free = src->free;

    return 0;
}

/**
 * Translate relative path to absolute path
 * @param abspath Absolute path.
 * @param relpath Relative path.
 * @return 0.
 */
int get_abspath(char *abspath, const char *relpath) {
    /**< If relpath is abspath. */
    if (!strcmp(relpath, DELIM) || relpath[0] == '/') {
        strcpy(abspath, relpath);
        return 0;
    }

    char str[PATHLENGTH];
    char *token;
    int i;
    ssize_t n;

    for (i = 0; i < PATHLENGTH; i++) {
        abspath[i] = '\0';
    }

    strcpy(str, relpath);
    token = strtok(str, DELIM);

    /**< Process the first token. */
    if (!strcmp(token, ".")) {
        /**< Cur folder. */
        if (current_dir[1] != '\0') {
            strcpy(abspath, current_dir);
        }
    } else if (!strcmp(token, "..")) {
        /**< Par folder. */
        if (current_dir[1] != '\0') {
            n = strlen(openfile_list[curdir].open_fcb.filename);
            strncpy(abspath, current_dir, strlen(current_dir) - n);
        }
    } else {
        strcpy(abspath, current_dir);
        if (current_dir[1] != '\0') {
            strcat(abspath, DELIM);
        }
        strcat(abspath, token);
    }

    /**< Process the other tokens. */
    token = strtok(NULL, DELIM);
    while (token != NULL) {
        strcat(abspath, DELIM);
        strcat(abspath, token);
        token = strtok(NULL, DELIM);
    }

    return 0;
}

/**
 * Find fcb by abspath
 * @param path
 * @return
 */
fcb *find_fcb(const char *path) {
    char abspath[PATHLENGTH];
    get_abspath(abspath, path);
    char *token = strtok(abspath, DELIM);
    if (token == NULL) {
        return (fcb *) (fs_head + BLOCK_SIZE * 5);
    }
    return find_fcb_r(token, 5);
}

/**
 * A procedure to find fcb recursively.
 * @param token File name in (ptr).
 * @param first Par fcb pointer.
 * @return FCB pointer of token.
 */
fcb *find_fcb_r(char *token, int first) {
    int i, length = BLOCK_SIZE;
    char fullname[NAMELENGTH] = "\0";
    fcb *root = (fcb *) (BLOCK_SIZE * first + fs_head);
    fcb *dir;
    block0 *init_block = (block0 *) fs_head;
    if (first == init_block->root) {
        length = ROOT_BLOCK_NUM * BLOCK_SIZE;
    }

    for (i = 0, dir = root; i < length / sizeof(fcb); i++, dir++) {
        if (dir->free == 0) {
            continue;
        }
        get_fullname(fullname, dir);
        if (!strcmp(token, fullname)) {
            token = strtok(NULL, DELIM);
            if (token == NULL) {
                return dir;
            }
            return find_fcb_r(token, dir->first);
        }
    }
    return NULL;
}

/**
 * Get a empty useropen entry.
 * @return If empty useropen exist return entry index, else return -1;
 */
int get_useropen() {
    int i;

    for (i = 0; i < MAX_OPENFILE; i++) {
        if (openfile_list[i].free == 0) {
            return i;
        }
    }

    return -1;
}

/**  */
void init_folder(int first, int second) {
    int i;
    fcb *par = (fcb *) (fs_head + BLOCK_SIZE * first);
    fcb *cur = (fcb *) (fs_head + BLOCK_SIZE * second);

    set_fcb(cur, ".", "di", 0, second, BLOCK_SIZE, 1);
    cur++;
    set_fcb(cur, "..", "di", 0, first, par->length, 1);
    cur++;
    for (i = 2; i < BLOCK_SIZE / sizeof(fcb); i++, cur++) {
        cur->free = 0;
    }
}

/**
 * Get file full name.
 * @param fullname A char array[NAMELENGTH].
 * @param fcb1 Source fcb pointer.
 */
void get_fullname(char *fullname, fcb *fcb1) {
    memset(fullname, '\0', NAMELENGTH);

    strcat(fullname, fcb1->filename);
    if (fcb1->attribute == 1) {
        strncat(fullname, ".", 2);
        strncat(fullname, fcb1->exname, 3);
    }
}

char *trans_date(char *sdate, unsigned short date) {
    int year, month, day;
    memset(sdate, '\0', 16);

    year = date & 0xfe00;
    month = date & 0x01e0;
    day = date & 0x001f;
    sprintf(sdate, "%04d-%02d-%02d", (year >> 9) + 1900, (month >> 5) + 1, day);
    return sdate;
}

char *trans_time(char *stime, unsigned short time) {
    int hour, min, sec;
    memset(stime, '\0', 16);

    hour = time & 0xfc1f;
    min = time & 0x03e0;
    sec = time & 0x001f;
    sprintf(stime, "%02d:%02d:%02d", hour >> 11, min >> 5, sec << 1);
    return stime;
}
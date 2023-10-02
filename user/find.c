#include "../kernel/types.h"
#include "user.h"
#include "../kernel/fs.h"
#include "../kernel/stat.h"

char *fmtname(char *path) {
    char *p;
    // 查找最后一个斜杠后的第一个字符
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;
    return p;
    }

void find(char *path, char *fname) {
    char buf[512], *p;          // buf用于存储路径，p用于读取路径
    int fd;                     // 文件描述符
    struct dirent de;           // 目录项
    struct stat st;             // 文件状态

    if ((fd = open(path, 0)) < 0) { // 打开文件失败
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) { // 获取文件状态失败
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {  // 判断文件类型
        case T_FILE:    // 文件
            if (strcmp(fmtname(path), fname) == 0) {
                printf("%s\n", path);
            }
            break;

        case T_DIR:    // 文件夹
            if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {   //判断路径长度是否超过512
                printf("find: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';

            while (read(fd, &de, sizeof(de)) == sizeof(de)) { // 读取目录项
                if (de.inum == 0) 
                    continue; // 文件夹中不存在任何文件
                if ((strcmp(de.name, ".") == 0) || (strcmp(de.name, "..") == 0)) 
                    continue; // 避免递归进入.和..

                memmove(p, de.name, DIRSIZ);    // 在路径后面加上文件名
                p[DIRSIZ] = 0;                  // 最后一位置0作为结束位

                if (stat(buf, &st) < 0) {
                    printf("ls: cannot stat %s\n", buf);
                    continue;
                }

                find(buf, fname);
            }
            break;
        default:
            break;
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Find needs two argument!\n");
        exit(-1);
    }
    find(argv[1], argv[2]);
    exit(0);
}
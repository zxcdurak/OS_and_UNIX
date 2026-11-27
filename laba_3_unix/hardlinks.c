#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <limits.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define HLEN 20

typedef struct {
    char *path;
    dev_t dev;
    ino_t ino;
    unsigned char h[HLEN];
} File;

File *files;
size_t count;
size_t capacity;

int hash_file(const char *path, unsigned char out[HLEN])
{
    int fd = open(path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) {
        return -1;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        close(fd);
        errno = ENOMEM;
        return -1;
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha1(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        close(fd);
        errno = EIO;
        return -1;
    }

    unsigned char buf[65536];
    unsigned int len = 0;
    ssize_t r;

    while ((r = read(fd, buf, sizeof buf)) > 0) {
        EVP_DigestUpdate(ctx, buf, r);
    }
    close(fd);

    int ok = (r == 0) && (EVP_DigestFinal_ex(ctx, out, &len) == 1);
    EVP_MD_CTX_free(ctx);
    return ok ? 0 : -1;
}

int copy_hash_of_same_inode(const struct stat *st, unsigned char out[HLEN])
{
    for (size_t i = 0; i < count; i++) {
        if (files[i].dev == st->st_dev && files[i].ino == st->st_ino) {
            memcpy(out, files[i].h, HLEN);
            return 1;
        }
    }
    return 0;
}

void free_files(void)
{
    for (size_t i = 0; i < count; i++) {
        free(files[i].path);
    }
    free(files);
    files = NULL;
    count = 0;
    capacity = 0;
}

int visit(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)ftw;

    if (flag != FTW_F) {
        return 0;
    }

    File f;
    f.dev = st->st_dev;
    f.ino = st->st_ino;

    int known = 0;
    if (st->st_nlink > 1) {
        known = copy_hash_of_same_inode(st, f.h);
    }
    if (!known && hash_file(path, f.h) < 0) {
        perror(path);
        return 0;
    }

    f.path = strdup(path);
    if (f.path == NULL) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    for (int i = 0; i < HLEN; i++) {
        printf("%02x", f.h[i]);
    }
    printf("  %s\n", path);

    if (count == capacity) {
        size_t new_capacity = capacity ? capacity * 2 : 128;
        File *grown = realloc(files, new_capacity * sizeof *files);
        if (grown == NULL) {
            fprintf(stderr, "out of memory\n");
            free(f.path);
            return 1;
        }
        files = grown;
        capacity = new_capacity;
    }
    files[count++] = f;
    return 0;
}

int compare(const void *a, const void *b)
{
    const File *x = a;
    const File *y = b;

    int r = memcmp(x->h, y->h, HLEN);
    if (r != 0) {
        return r;
    }
    return strcmp(x->path, y->path);
}

int replace_with_link(const char *orig, const char *dup)
{
    char tmp[PATH_MAX + 16];
    int written = snprintf(tmp, sizeof tmp, "%s.linkstemp", dup);
    if (written < 0 || (size_t)written >= sizeof tmp) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (link(orig, tmp) < 0) {
        return -1;
    }
    if (rename(tmp, dup) < 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <dir>\n", argv[0]);
        return 2;
    }

    int rc = nftw(argv[1], visit, 32, FTW_PHYS);
    if (rc != 0) {
        if (rc < 0) {
            perror(argv[1]);
        }
        free_files();
        return 1;
    }

    if (count > 1) {
        qsort(files, count, sizeof *files, compare);
    }

    int replaced = 0;
    size_t i = 0;
    while (i < count) {
        size_t j = i + 1;
        while (j < count && memcmp(files[i].h, files[j].h, HLEN) == 0) {
            j++;
        }

        for (size_t k = i + 1; k < j; k++) {
            if (files[k].dev != files[i].dev) {
                continue;
            }
            if (files[k].ino == files[i].ino) {
                continue;
            }

            if (replace_with_link(files[i].path, files[k].path) < 0) {
                perror(files[k].path);
            } else {
                printf("link: %s -> %s\n", files[k].path, files[i].path);
                replaced++;
            }
        }
        i = j;
    }

    printf("replaced: %d\n", replaced);
    free_files();
    return 0;
}

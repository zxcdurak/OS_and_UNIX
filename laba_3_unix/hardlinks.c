#define _GNU_SOURCE
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

    unsigned char buf[65536];
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned int len = 0;
    ssize_t r;

    EVP_DigestInit_ex(ctx, EVP_sha1(), NULL);
    while ((r = read(fd, buf, sizeof buf)) > 0) {
        EVP_DigestUpdate(ctx, buf, r);
    }
    close(fd);

    int ok = (r == 0) && (EVP_DigestFinal_ex(ctx, out, &len) == 1);
    EVP_MD_CTX_free(ctx);
    return ok ? 0 : -1;
}

int visit(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)ftw;

    if (flag != FTW_F || !S_ISREG(st->st_mode)) {
        return 0;
    }

    File f;
    f.path = strdup(path);
    f.dev = st->st_dev;
    f.ino = st->st_ino;

    if (hash_file(path, f.h) < 0) {
        perror(path);
        free(f.path);
        return 0;
    }

    for (int i = 0; i < HLEN; i++) {
        printf("%02x", f.h[i]);
    }
    printf("  %s\n", path);

    if (count == capacity) {
        capacity = capacity ? capacity * 2 : 128;
        files = realloc(files, capacity * sizeof *files);
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
    snprintf(tmp, sizeof tmp, "%s.linkstemp", dup);

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

    if (nftw(argv[1], visit, 32, FTW_PHYS) < 0) {
        perror(argv[1]);
        return 1;
    }

    qsort(files, count, sizeof *files, compare);

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
    return 0;
}
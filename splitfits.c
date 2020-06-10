#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#define SPLITFITS_BLOCK 2880

struct SplitFITS {
    FILE *handle;
    char *path_origin;
    char *path_header;
    char *path_data;
    size_t header_size;
    size_t data_size;
};

int splitfits_header_write(struct SplitFITS **ctx) {
    char *block;
    FILE *fp;
    size_t count = 0;
    size_t bytes;

    fp = fopen((*ctx)->path_header, "w+b");
    if (fp == NULL) {
        perror((*ctx)->path_header);
        return 1;
    }
    fseek((*ctx)->handle, 0, SEEK_SET);

    block = calloc(SPLITFITS_BLOCK + 1, sizeof(char));

    while (1) {
        memset(block, ' ', SPLITFITS_BLOCK);
        bytes = fread(block, sizeof(char), SPLITFITS_BLOCK, (*ctx)->handle);

        if (strstr(block, "END     ") != NULL) {
            break;
        }

        if (bytes == 0) {
            break;
        }

        if (fwrite(block, sizeof(char), SPLITFITS_BLOCK, fp) == 0) {
            perror("short write");
            return 1;
        }
        count++;
    }

    (*ctx)->header_size = SPLITFITS_BLOCK * (count);

    fclose(fp);
    free(block);
    return 0;
}

int splitfits_data_write(struct SplitFITS **ctx) {
    char *block;
    FILE *fp;
    fp = fopen((*ctx)->path_data, "w+b");

    size_t size = 0;
    fseek((*ctx)->handle, 0, SEEK_END);
    size = ftell((*ctx)->handle) - (*ctx)->header_size;
    fseek((*ctx)->handle, (*ctx)->header_size, SEEK_SET);

    block = calloc(SPLITFITS_BLOCK + 1, sizeof(char));

    while (fread(block, sizeof(char), SPLITFITS_BLOCK, (*ctx)->handle) != 0) {
        if (fwrite(block, sizeof(char), SPLITFITS_BLOCK, fp) == 0) {
            perror("short write");
            return 1;
        }
    }

    (*ctx)->data_size = size;

    fclose(fp);
    free(block);
    return 0;
}

int splitfits_split(struct SplitFITS **ctx) {
    if (splitfits_header_write(ctx) != 0 || splitfits_data_write(ctx) != 0) {
        return 1;
    }
    return 0;
}

void splitfits_free(struct SplitFITS *ctx) {
    free(ctx->path_origin);
    free(ctx->path_data);
    free(ctx->path_header);
    fclose(ctx->handle);
    free(ctx);
}

void splitfits_show(struct SplitFITS *ctx) {
    printf("%s:\n", ctx->path_origin);
    printf("\t%-20s (%zu bytes)\n", ctx->path_header, ctx->header_size);
    printf("\t%-20s (%zu bytes)\n", ctx->path_data, ctx->data_size);
}

int splitfits_combine(const char *headerfile, const char *datafile) {
    FILE *handle[] = {
            fopen(headerfile, "rb"),
            fopen(datafile, "rb"),
    };
    FILE *fp;
    size_t handle_count;
    char *suffix;
    char tempfile[PATH_MAX];
    char outfile[PATH_MAX];

    handle_count = sizeof(handle) / sizeof(FILE *);
    sprintf(tempfile, "%s.tmp_XXXXXX", __FUNCTION__);

    if (mkstemp(tempfile) < 0) {
        perror(tempfile);
        return 1;
    }

    fp = fopen(tempfile, "w+b");
    if (fp == NULL) {
        perror("could not open temporary file for writing");
        return 1;
    }

    char block[2] = {0, 0};
    size_t bytes = 0;
    for (size_t i = 0; i < handle_count; i++) {
        while (1) {
            bytes = fread(block, sizeof(char), 1, handle[i]);
            if (bytes) {
                fwrite(block, sizeof(char), 1, fp);
            } else {
                break;
            }
        }
        fclose(handle[i]);
    }
    rewind(fp);

    strcpy(outfile, headerfile);
    suffix = strstr(outfile, "_hdr.txt");
    if (suffix == NULL) {
        fprintf(stderr, "%s: does not have the correct suffix (_hdr.txt)\n");
        return 1;
    }

    *suffix = '\0';
    strcat(suffix, ".fits");

    printf("Writing: %s\n", outfile);

    FILE *ofp;
    ofp = fopen(outfile, "w+b");
    if (ofp == NULL) {
        perror(outfile);
        return 1;
    }

    while (fread(block, sizeof(char), 1, fp) != 0) {
        fwrite(block, sizeof(char), 1, ofp);
    }

    fclose(ofp);
    fclose(fp);
    remove(tempfile);
    return 0;
}

struct SplitFITS *splitfits(const char *_filename) {
    struct SplitFITS *ctx;
    char *filename;
    char *ext;

    ctx = calloc(1, sizeof(struct SplitFITS));
    if (ctx == NULL) {
        perror("calloc");
        exit(1);
    }

    ctx->handle = fopen(_filename, "r+b");
    if (ctx->handle == NULL) {
        perror(_filename);
        exit(1);
    }

    ctx->path_origin = strdup(_filename);
    filename = strdup(_filename);

    ctx->path_header = calloc(PATH_MAX, sizeof(char));
    ext = strrchr(filename, '.');

    if (ext == NULL) {
        fprintf(stderr, "%s: has no file extension\n", ctx->path_origin);
    } else {
        *ext = '\0';
    }

    strcpy(ctx->path_header, filename);
    strcat(ctx->path_header, "_hdr.txt");

    ctx->path_data = calloc(PATH_MAX, sizeof(char));
    strcpy(ctx->path_data, filename);
    strcat(ctx->path_data, "_data.bin");

    free(filename);
    return ctx;
}

int main(int argc, char *argv[]) {
    int bad_files;
    char *prog;

    prog = strrchr(argv[0], '/');
    if (prog == NULL) {
        prog = argv[0];
    } else {
        prog++;
    }

    if (argc < 2) {
        printf("usage: %s {[-c HEADER_FILE DATA_FILE] | FILE(s)}", prog);
        exit(1);
    }

    if (strcmp(argv[1], "-c") == 0) {
        int combine;
        const char *header_file = argv[2];
        const char *data_file = argv[3];

        if (argc < 4) {
            fprintf(stderr, "-c requires two arguments (HEADER FILE and DATA_FILE)");
            exit(1);
        }

        if (access(header_file, F_OK) != 0) {
            fprintf(stderr, "%s: header file does not exist\n", header_file);
            exit(1);
        }

        if (access(data_file, F_OK) != 0) {
            fprintf(stderr, "%s: data file does not exist\n", header_file);
            exit(1);
        }

        combine = splitfits_combine(header_file, data_file);
        exit(combine);
    }

    bad_files = 0;
    for (size_t i = 1; i < argc; i++) {
        if (access(argv[i], F_OK) != 0) {
            fprintf(stderr, "%s: does not exist", argv[i]);
            bad_files = 1;
        }
    }

    if (bad_files) {
        fprintf(stderr, "Exiting...\n");
        exit(1);
    }

    for (size_t i = 1; i < argc; i++) {
        struct SplitFITS *fits;
        fits = splitfits(argv[i]);

        if (splitfits_split(&fits) != 0) {
            fprintf(stderr, "%s: split failed\n", fits->path_origin);
        }

        splitfits_show(fits);
        splitfits_free(fits);
    }
    return 0;
}

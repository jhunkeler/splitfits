#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>

#define FITS_BLOCK 2880
#define FITS_RECORD 80

/**
 * Check for FITS header keyword in data block
 * @param block of memory of size FITS_BLOCK
 * @param key keyword to search for
 * @return 0=not found, 1=found
 */
int has_key(char *block, const char *key) {
    for (size_t i = 0; i < FITS_BLOCK; i += FITS_RECORD) {
        char record[FITS_RECORD];
        memcpy(record, block + i, FITS_RECORD - 1);
        record[FITS_RECORD - 1] = '\0';
        if (strncmp(record, key, strlen(key)) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * Check if a data block is a FITS header
 * @param block
 * @return 0=no, 1=yes
 */
int is_header_start(char *block) {
    if (has_key(block, "SIMPLE") || has_key(block, "XTENSION")) {
        return 1;
    }
    return 0;
}

/**
 * Check if a data block is the end of a FITS header
 * @param block
 * @return 0=no, 1=yes
 */
int is_header_end(char *block) {
    if (has_key(block, "END") && block[FITS_BLOCK - 1] == ' ') {
        return 1;
    }
    return 0;
}

/**
 * Obtain the last element of a filesystem path
 * @param path
 * @return
 */
char *get_basename(char *path) {
    char *sep;
    if ((sep = strrchr(path, '/')) == NULL) {
        return path;
    }
    sep++;
    path = sep;
    return path;
}

/**
 * Obtain the parent directory of a file path
 * @param path
 * @return
 */
char *get_dirname(char *path) {
    char *sep;
    if ((sep = strrchr(path, '/')) == NULL) {
        return path;
    }
    *sep = '\0';
    return path;
}

/**
 * Describe the start/stop offsets for each frame in a FITS file
 */
struct DataFrame {
    size_t *start;      // Array of starting offsets
    size_t *stop;       // Array of stopping (end) offsets
    size_t num_inuse;   // Number of records used
    size_t num_alloc;   // Number of records allocated
};

struct DataFrame *dataframe_init() {
    struct DataFrame *ctx;
    ctx = calloc(1, sizeof(struct DataFrame));
    ctx->num_inuse = 0;
    ctx->num_alloc = 2;

    ctx->start = calloc(ctx->num_alloc, sizeof(size_t));
    ctx->stop = calloc(ctx->num_alloc, sizeof(size_t));
    return ctx;
}

void dataframe_new(struct DataFrame **ctx) {
    size_t *tmp;
    tmp = realloc((*ctx)->start, ((*ctx)->num_alloc + 1) * sizeof(size_t *));
    if (tmp == NULL) {
        perror("realloc start");
        exit(1);
    }
    (*ctx)->start = tmp;

    tmp = realloc((*ctx)->stop, ((*ctx)->num_alloc + 1) * sizeof(size_t *));
    if (tmp == NULL) {
        perror("realloc stop");
        exit(1);
    }
    (*ctx)->stop = tmp;

    (*ctx)->num_alloc += 2;
    (*ctx)->num_inuse += 2;
}

int split_file(const char *_filename, const char *dest) {
    FILE *fp_in;
    FILE *fp_out;
    FILE *map_out;
    char outfile[PATH_MAX];
    char _mapfile[PATH_MAX];
    char *mapfile;
    char *block;
    size_t bytes_read, bytes_write;
    size_t block_size;
    size_t filepos;
    int i, done;
    struct DataFrame *dataFrame, *hb;

    bytes_read = 0;
    bytes_write = 0;
    i = 0;
    done = 0;
    block = calloc(FITS_BLOCK, sizeof(char));
    fp_in = fopen(_filename, "rb");

    mapfile = _mapfile;
    sprintf(mapfile, "%s/%s.map", dest ? dest : ".", _filename);

    filepos = 0;
    block_size = FITS_BLOCK;
    dataFrame = dataframe_init();

    size_t off;
    off = 0;
    map_out = fopen(mapfile, "w+");
    while (!done) {
        while (1) {
            filepos = ftell(fp_in);
            bytes_read = fread(block, sizeof(char), block_size, fp_in);
            if (bytes_read < 1) {
                done = 1;
                break;
            }

            if (is_header_start(block)) {
                dataFrame->start[dataFrame->num_inuse] = filepos;
            }

            if (is_header_end(block)) {
                filepos = ftell(fp_in);
                dataFrame->stop[dataFrame->num_inuse] = filepos;
                break;
            }
        }
        off += 2;

        if (!done) {
            dataframe_new(&dataFrame);
        }
    }

    size_t last;
    for (off = 0; off < dataFrame->num_inuse; off += 2) {
        if (off < 2) {
            continue;
        }

        size_t size;
        size = dataFrame->start[off] - dataFrame->stop[off - 2];
        last = off - 1;

        dataFrame->start[last] = dataFrame->stop[off - 2];
        dataFrame->stop[last] = dataFrame->start[off];
    }
    dataFrame->start[off - 1] = dataFrame->stop[off - 2];
    fseek(fp_in, 0L, SEEK_END);
    dataFrame->stop[off - 1] = ftell(fp_in);

    printf("info:\n");
    for (size_t d = 0; d < dataFrame->num_inuse; d++) {
        printf("%zu: start: %zu, stop: %zu\n", d, dataFrame->start[d], dataFrame->stop[d]);
    }

    done = 0;
    rewind(fp_in);

    size_t data_size;
    data_size = 0;
    for (off = 0; off < dataFrame->num_inuse; off++) {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        char *ext;

        strcpy(path, _filename);
        if (dest != NULL) {
            strcpy(path, dest);
        } else {
            get_dirname(path);
        }

        strcpy(filename, _filename);
        get_basename(filename);

        if (strcmp(path, filename) == 0) {
            strcpy(path, ".");
        }

        sprintf(outfile, "%s/%s", path, filename);
        if ((ext = strrchr(outfile, '.')) == NULL) {
            fprintf(stderr, "%s: does not have an extension\n", outfile);
        } else {
            *ext = '\0';
        }

        if (dataFrame->start[off] == dataFrame->stop[off]) {
            printf("skipped %d: identical begin/end offset\n", i);
            i++;
            continue;
        }

        sprintf(outfile + strlen(outfile), ".part_%d", i);
        printf("creating %s\n", outfile);
        fp_out = fopen(outfile, "w+b");

        filepos = ftell(fp_in);
        block_size = FITS_BLOCK;

        fprintf(map_out, "%zu:", filepos);
        fseek(fp_in, dataFrame->start[off], SEEK_SET);
        while(1) {
            filepos = ftell(fp_in);

            if (filepos == dataFrame->stop[off]) {
                break;
            }

            bytes_read = fread(block, sizeof(char), block_size, fp_in);
            if (bytes_read < 1) {
                done = 1;
                break;
            }

            bytes_write = fwrite(block, sizeof(char), block_size, fp_out);
            if (bytes_write < 1) {
                perror("write failure");
                exit(1);
            }
        }

        fclose(fp_out);

        char *bname = get_basename(outfile);
        fprintf(map_out, "%zu:%s\n", filepos, bname);
        i++;
    }
    fclose(map_out);
    return 0;
}

int combine_file(const char *_filename, const char *dest) {
    size_t bytes;
    char buffer[PATH_MAX];
    char outfile[PATH_MAX];
    char *block;
    char *ext;
    FILE *fp_in;
    FILE *fp_out;

    block = calloc(FITS_BLOCK, sizeof(char));

    fp_in = fopen(_filename, "r");
    if (fp_in == NULL) {
        perror(_filename);
        exit(1);
    }

    char *filename;
    char path[PATH_MAX];

    filename = calloc(PATH_MAX, sizeof(char));

    strcpy(filename, _filename);
    filename = get_basename(filename);

    if (dest == NULL) {
        strcpy(path, ".");
    } else {
        strcpy(path, dest);
    }

    sprintf(outfile, "%s/%s", path, filename);

    ext = strrchr(outfile, '.');
    if (ext != NULL) {
        *ext = '\0';
    }

    fp_out = fopen(outfile, "w+b");
    if (fp_out == NULL) {
        perror(outfile);
        exit(1);
    }

    printf("Writing: %s\n", outfile);
    while (fscanf(fp_in, "%s\n", buffer) > 0) {
        char *mark;
        char *name;
        FILE *fp_tmp;

        mark = strrchr(buffer, ':');
        if (mark != NULL) {
            mark++;
            name = strdup(mark);
        }

        fp_tmp = fopen(name, "r");
        if (fp_tmp == NULL) {
            perror(name);
            exit(1);
        }

        printf("Reading: %s\n", name);
        while (1) {
            if (fread(block, sizeof(char), FITS_BLOCK, fp_tmp) < 1) {
                break;
            }

            if (fwrite(block, sizeof(char), FITS_BLOCK, fp_out) < 1) {
                perror("write failure");
                break;
            }
        }
        fclose(fp_tmp);
    }
    fclose(fp_in);
    fclose(fp_out);
    free(block);
    return 0;
}

int main(int argc, char *argv[]) {
    int bad_files;
    char *prog;
    char *outdir;

    prog = strrchr(argv[0], '/');
    outdir = NULL;

    if (prog == NULL) {
        prog = argv[0];
    } else {
        prog++;
    }

    if (argc < 2) {
        printf("usage: %s [-o DIR] {[-c MAP_FILE] | FILE(s)}\n", prog);
        printf(" Options:\n");
        printf("   -c  --combine    Reconstruct original file using .map data\n");
        printf("   -o  --outdir     Path where output files are stored\n");
        exit(1);
    }

    size_t inputs;
    for (inputs = 1; inputs < argc; inputs++) {
        if (strcmp(argv[inputs], "-o") == 0 || strcmp(argv[inputs], "--outdir") == 0) {
            inputs++;
            if (access(argv[inputs], R_OK | W_OK | X_OK) != 0) {
                fprintf(stderr, "%s: output directory does not exist or is not writable\n", argv[inputs]);
            }
            outdir = strdup(argv[inputs]);
            continue;
        }

        if (strcmp(argv[inputs], "-c") == 0 || strcmp(argv[inputs], "--combine") == 0) {
            if (argc < 3) {
                fprintf(stderr, "-c|--combine requires an argument (MAP file)");
                exit(1);
            }

            int combine;
            inputs++;
            const char *map_file = argv[inputs];

            if (access(map_file, F_OK) != 0) {
                fprintf(stderr, "%s: data file does not exist\n", map_file);
                exit(1);
            }

            combine = combine_file(map_file, outdir);
            exit(combine);
        }
        break;
    }

    bad_files = 0;
    for (size_t i = inputs; i < argc; i++) {
        if (access(argv[i], F_OK) != 0) {
            fprintf(stderr, "%s: does not exist\n", argv[i]);
            bad_files = 1;
        }
    }

    if (bad_files) {
        fprintf(stderr, "Exiting...\n");
        exit(1);
    }

    if (outdir != NULL && access(outdir, F_OK) != 0) {
        fprintf(stderr, "%s: %s\n", outdir, strerror(errno));
        exit(1);
    }

    for (size_t i = inputs; i < argc; i++) {
        split_file(argv[i], outdir);
    }

    if (outdir != NULL) {
        free(outdir);
    }
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#define OP_SPLIT 0
#define OP_COMBINE 1
#define FITS_BLOCK 2880
#define FITS_RECORD 80

static int mkdirs(const char *_path, mode_t mode) {
    const char *delim;
    char *parts;
    char *token;
    char path[PATH_MAX];
    int status;

    delim = "/";
    parts = strdup(_path);
    if (parts == NULL) {
        perror(parts);
        exit(1);
    }

    memset(path, '\0', PATH_MAX);

    status = -1;
    token = NULL;
    while ((token = strsep(&parts, delim)) != NULL) {
        strcat(path, token);
        strcat(path, delim);
        status = mkdir(path, mode);
        if (status < 0) {
            break;
        }
    }
    return status;
}

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

/**
 * Initialize `DataFrame` structure
 * @return `DataFrame`
 */
struct DataFrame *dataframe_init() {
    struct DataFrame *ctx;
    ctx = calloc(1, sizeof(struct DataFrame));
    if (ctx == NULL) {
        perror("DataFrame");
        exit(1);
    }

    ctx->num_inuse = 0;
    ctx->num_alloc = 2;

    ctx->start = calloc(ctx->num_alloc, sizeof(size_t));
    ctx->stop = calloc(ctx->num_alloc, sizeof(size_t));

    if (ctx->start == NULL || ctx->stop == NULL) {
        perror("could not allocate array");
        exit(1);
    }
    return ctx;
}

/**
 * Allocate another record for offsets in `start` and `stop` arrays
 * @param ctx address of `DataFrame` structure
 */
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

/**
 * Compile a listing of start/stop offsets. Write each chunk as an individual file.
 * @param _filename FITS file
 * @param dest path (may be NULL)
 * @return
 */
int split_file(const char *_filename, const char *dest) {
    FILE *fp_in;
    FILE *fp_out;
    FILE *map_out;
    char outfile[PATH_MAX];
    char path[PATH_MAX];
    char filename[PATH_MAX];
    char _mapfile[PATH_MAX];
    char *mapfile;
    char *block;
    size_t bytes_read, bytes_write;
    size_t block_size;
    size_t filepos;
    int i, done;
    struct DataFrame *dataFrame;

    i = 0;
    done = 0;

    block = calloc(FITS_BLOCK, sizeof(char));
    if (block == NULL) {
        perror("block buffer");
        exit(1);
    }

    fp_in = fopen(_filename, "rb");
    if (fp_in == NULL) {
        perror(_filename);
        exit(1);
    }

    mapfile = _mapfile;
    strcpy(filename, _filename);
    sprintf(mapfile, "%s/%s.part_map", dest ? dest : ".", get_basename(filename));

    block_size = FITS_BLOCK;
    dataFrame = dataframe_init();

    size_t off;
    off = 0;

    map_out = fopen(mapfile, "w+");
    if (map_out == NULL) {
        perror(mapfile);
        exit(1);
    }
    printf("Map: %s\n", mapfile);

    // Outer loop increments .partN file counter per iteration
    while (!done) {
        // Read input file and collate header start/stop offets
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
                // Move on to next part
                break;
            }
        }

        // Allocate a new record when we are NOT DONE reading the input file
        if (!done) {
            dataframe_new(&dataFrame); // allocates TWO records per call
        }
    }

    // Fill in ODD record gaps (EVEN records are header data) with data between headers
    for (off = 0; off < dataFrame->num_inuse; off += 2) {
        // Ignore first pair because it's guaranteed to be a header
        if (off < 2) {
            continue;
        }

        // Assign ODD offsets
        dataFrame->start[off - 1] = dataFrame->stop[off - 2];
        dataFrame->stop[off - 1] = dataFrame->start[off];
    }
    // Assign final offset leading up the end of the file
    dataFrame->start[off - 1] = dataFrame->stop[off - 2];
    fseek(fp_in, 0L, SEEK_END);
    dataFrame->stop[off - 1] = ftell(fp_in);

    // Reuse input file handle
    rewind(fp_in);

    // Read offset from the input files and write it to its respective .part_N file
    for (off = 0; off < dataFrame->num_inuse; off++) {
        char *ext;

        // Get dirname of input path
        strcpy(path, _filename);
        if (dest != NULL) {
            strcpy(path, dest);
        } else {
            get_dirname(path);
        }

        // Get basename of input file
        strcpy(filename, _filename);
        get_basename(filename);

        // When the basename and dirname are the same, use the current working directory path
        if (strcmp(path, filename) == 0) {
            strcpy(path, ".");
        }

        // Create output file name
        sprintf(outfile, "%s/%s", path, get_basename(filename));

        // Strip file extension from output file
        if ((ext = strrchr(outfile, '.')) == NULL) {
            fprintf(stderr, "%s: does not have an extension\n", outfile);
        } else {
            *ext = '\0';
        }

        // When headers physically border one another, this can happen
        if (dataFrame->start[off] == dataFrame->stop[off]) {
            printf("skipped %d: identical begin/end offset\n", i);
            i++;
            continue;
        }

        // Finalize output file name
        sprintf(outfile + strlen(outfile), ".part_%d", i);

        fp_out = fopen(outfile, "w+b");
        if (fp_out == NULL) {
            perror(outfile);
            exit(1);
        }

        block_size = FITS_BLOCK;

        // Seek to first offset (probably zero)
        fseek(fp_in, dataFrame->start[off], SEEK_SET);
        filepos = ftell(fp_in);
        // Write start offset to map
        fprintf(map_out, "%zu:", filepos);

        // Read (input) / write (part) for each offset in the data frame
        while(1) {
            filepos = ftell(fp_in);

            if (filepos == dataFrame->stop[off]) {
                break;
            }

            bytes_read = fread(block, sizeof(char), block_size, fp_in);
            if (bytes_read < 1) {
                break;
            }

            bytes_write = fwrite(block, sizeof(char), block_size, fp_out);
            if (bytes_write < 1) {
                perror("write failure");
                exit(1);
            }
        }

        printf("Writing: %s\n", outfile);
        fclose(fp_out);

        // Record output file offset and basename in the map
        char *bname;
        bname = get_basename(outfile);
        fprintf(map_out, "%zu:%s\n", filepos, bname);

        // Next part
        i++;
    }
    fclose(map_out);
    return 0;
}

/**
 * Reconstruct a file using a .part_map file
 * @param _filename path to .part_map file
 * @param dest path to store reconstructed file (may be NULL)
 * @return
 */
int combine_file(const char *_filename, const char *dest) {
    char buffer[PATH_MAX];
    char path[PATH_MAX];
    char outfile[PATH_MAX];
    char *filename;
    char *dirpath;
    char *block;
    char *ext;
    FILE *fp_in;
    FILE *fp_out;

    block = calloc(FITS_BLOCK, sizeof(char));
    if (block == NULL) {
        perror("block buffer");
        exit(1);
    }

    fp_in = fopen(_filename, "r");
    if (fp_in == NULL) {
        perror(_filename);
        exit(1);
    }

    filename = calloc(PATH_MAX, sizeof(char));
    if (filename == NULL) {
        perror("filename");
        exit(1);
    }

    dirpath = calloc(PATH_MAX, sizeof(char));
    if (dirpath == NULL) {
        perror("dirpath");
        exit(1);
    }

    strcpy(filename, _filename);
    strcpy(dirpath, _filename);
    filename = get_basename(filename);
    dirpath = get_dirname(dirpath);

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

    printf("Map: %s\n", _filename);
    while (fscanf(fp_in, "%s\n", buffer) > 0) {
        char *mark;
        char *name;
        FILE *fp_tmp;

        // Allocate enough room to store path to file name stored in map
        name = calloc(PATH_MAX, sizeof(char));
        if (name == NULL) {
            perror("name buffer");
            exit(1);
        }

        // Append the dirname where the map file was located
        strcpy(name, dirpath);

        // Get .part_N file name
        mark = strrchr(buffer, ':');
        if (mark != NULL) {
            mark++;
            // Append the file name to the path
            strcat(name, "/");
            strcat(name, mark);
        }

        // Open .part_N for reading
        fp_tmp = fopen(name, "r");
        if (fp_tmp == NULL) {
            perror(name);
            exit(1);
        }

        // Append .part_N data to the output file sequentially
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
    printf("Writing: %s\n", outfile);
    fclose(fp_in);
    fclose(fp_out);
    free(block);
    return 0;
}

void usage(char *program_name) {
    printf("usage: %s [-o DIR] [-c] {FILE(s)}\n", program_name);
    printf(" Options:\n");
    printf("   -h  --help       This message\n");
    printf("   -c  --combine    Reconstruct original file using .part_map data\n");
    printf("   -o  --outdir     Path where output files are stored\n");
}

int main(int argc, char *argv[]) {
    int bad_files;
    char *prog;
    char *outdir;
    int op_mode;

    // Set program name
    prog = get_basename(argv[0]);

    // Output directory (default of NULL indicates "current directory");
    outdir = NULL;
    op_mode = OP_SPLIT;

    // Check program argument count
    if (argc < 2) {
        usage(prog);
        exit(1);
    }

    // Parse program arguments
    size_t inputs;
    for (inputs = 1; inputs < argc; inputs++) {
        // User-defined output directory
        if (strcmp(argv[inputs], "-h") == 0 || strcmp(argv[inputs], "--help") == 0) {
            usage(prog);
            exit(0);
        } else if (strcmp(argv[inputs], "-o") == 0 || strcmp(argv[inputs], "--outdir") == 0) {
            inputs++;
            if (argv[inputs] == NULL) {
                fprintf(stderr, "-o requires an argument\n");
                usage(prog);
                exit(1);
            }
            if (access(argv[inputs], R_OK | W_OK | X_OK) != 0) {
                printf("Creating output directory: %s\n", argv[inputs]);
                if (mkdirs(argv[inputs], 0755) < 0) {
                    perror(outdir);
                    exit(1);
                }
            }
            outdir = strdup(argv[inputs]);
        } else if (strcmp(argv[inputs], "-c") == 0 || strcmp(argv[inputs], "--combine") == 0) {
            // User wants to reconstruct a FITS file using a .part_map
            op_mode = OP_COMBINE;
        } else {
            // Arguments beyond this point are considered input file paths
            break;
        }
    }

    // Make sure we have at least one file to process
    if (argc - inputs == 0) {
        fprintf(stderr, "At least one input FILE is required\n");
        usage(prog);
        exit(1);
    }

    // Make sure all input files exist
    bad_files = 0;
    for (size_t i = inputs; i < argc; i++) {
        if (access(argv[i], F_OK) != 0) {
            fprintf(stderr, "%s: does not exist\n", argv[i]);
            bad_files = 1;
        }
    }

    // If not all input files exist, then die
    if (bad_files) {
        fprintf(stderr, "Exiting...\n");
        exit(1);
    }

    // Make sure user-defined output directory exists
    if (outdir != NULL && access(outdir, F_OK) != 0) {
        fprintf(stderr, "%s: %s\n", outdir, strerror(errno));
        exit(1);
    }

    // Split or combine files based on user input
    for (size_t i = inputs; i < argc; i++) {
        if (op_mode != OP_COMBINE) {
            split_file(argv[i], outdir);
        } else {
            combine_file(argv[i], outdir);
        }
    }

    // Clean up
    if (outdir != NULL) {
        free(outdir);
    }
    return 0;
}

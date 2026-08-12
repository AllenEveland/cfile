// ### Header ### //
#define _GNU_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <inttypes.h>
#include <linux/limits.h>
#include <linux/fs.h>
#include <magic.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


// ### Struct ### //
struct FILEMETADATA {
    // File
    uint32_t count_file;
    uint32_t cap_file;
    char **filename;
};

struct BASIC_INFORMATION {
    char* title;

    char* filename;

    struct {
        char* filetype;
        char* mimetype;
    } type;

    char actualpath[PATH_MAX];
};


struct human_readable_size {
    double value;
    char* unit;
};

struct block_allocator {
    int64_t blk_allocate;
    int32_t io_blk_size;
};

struct STORAGE_DETAILS {
    char *title;

    ino_t inode_number;
    nlink_t hardlink_number;

    // bytes size and human-readable size
    struct {
        uint64_t bytes_size;

        struct human_readable_size hrd_size;
    } size;

    struct block_allocator blk_allocator;

    long long device_id;
    int sparse_file;
};

struct filemode {
    mode_t octal_access_mode;
    char symbolic_permission[11];
};

struct owner_group {
    uid_t owner_id;
    gid_t group_id;

    struct passwd *name_of_owner;
    struct group *name_of_group;
};

struct special_permision {
    mode_t suid;
    mode_t sgid;

    mode_t sticky_bits;
};

struct SECURITY_OWNERSHIP {
    char *title;

    // file mode
    struct filemode fmode;

    // owner - group
    struct owner_group own_grp;

    // special permission
    struct special_permision spec_perm;
};

struct time_file_chrono {
    char *born;
    char *access;
    char *modify;
    char *change;
};

struct TIME_CHORONOLOGY {
    char *title;

    struct time_file_chrono time_file;
};


// ### Function ### //
void Usage(void) {
    printf("Usage: cfile file...\n");
}

int is_directory(const char *path) {
    struct stat path_stat;
    
    // stat returns 0 on success; if it fails, the path does not exist or is inaccessible
    if (stat(path, &path_stat) != 0) {
        return 0; 
    }
    
    // Check if the file system object is a directory
    return S_ISDIR(path_stat.st_mode);
}

int check_file_valid(char *filename) {
    if (filename == NULL) {
        fprintf(stderr, "\x1b[31mERROR\x1b[0m: No filename provided.\n");
        return 0;
    }
    if (access(filename, F_OK) != 0) {
        fprintf(stderr, "\x1b[31mERROR\x1b[0m: File not found: %s\n", filename);
        return 0;
    }
    if (access(filename, R_OK) != 0) {
        fprintf(stderr, "\x1b[31mERROR\x1b[0m: File not readable: %s\n", filename);
        return 0;
    }
    if(is_directory(filename)) {
        fprintf(stderr, "\x1b[31mERROR\x1b[0m: The directory check feature is not yet supported: %s\n", filename);
        return 0;
    }

    return 1;
}

int ArgParse(struct FILEMETADATA *FileMetadata, int argc, char *argv[]) {
    for (int tok = 1; tok < argc; tok++) {
        if (check_file_valid(argv[tok])) {
            if (FileMetadata->count_file >= FileMetadata->cap_file) {
                uint32_t new_cap = FileMetadata->cap_file * 2;
                char **tmp = realloc(FileMetadata->filename, new_cap * sizeof(char));
                if (!tmp) {
                    printf("\x1b[31mERROR\x1b[0m: Memory allocation failed at ArgParse\n");
                    return 1;
                }
                FileMetadata->filename = tmp;
                FileMetadata->cap_file = new_cap;
            }
            FileMetadata->filename[FileMetadata->count_file] = malloc(strlen(argv[tok]));               // Allocate memory for save filename
            memcpy(FileMetadata->filename[FileMetadata->count_file], argv[tok], strlen(argv[tok]));     // Copy filename to destination
            FileMetadata->count_file += 1;                                                              // Increase count
        }
        else {
            // printf("\x1b[31mERROR\x1b[0m: Invalid input with %s\n", argv[tok]);
            return 1;
        }
    }
    return 0;
}

void MemFree(struct FILEMETADATA *FileMetadata) {
    for(uint32_t i = 0; i < FileMetadata->count_file; i++) {
        free(FileMetadata->filename[i]);
    }
    FileMetadata->count_file = 0;
    FileMetadata->cap_file = 0;
    free(FileMetadata->filename);
}

char *MIME_file_type(const char* file) {
    magic_t magic_file_checking;
    const char *mime_type = NULL;
    magic_file_checking = magic_open(MAGIC_MIME_TYPE);

    // Checking magic initialization
    if(magic_file_checking == NULL) {
        // printf("\x1b[31mERROR\x1b[0m: Can not initialize magic library\n");
        return NULL;
    }

    // Loading operating system database
    if(magic_load(magic_file_checking, NULL) != 0) {
        // printf("\x1b[31mERROR\x1b[0m: Can not load database");
        magic_close(magic_file_checking);
        return NULL;
    }

    // Reading type of file
    mime_type = magic_file(magic_file_checking, file);
    if(mime_type == NULL) {
        // printf("\x1b[31mERROR\x1b[0m: %s\n", magic_error(magic_file_checking));
        return NULL;
    }

    char *mime_type_ret = strdup(mime_type);
    magic_close(magic_file_checking);

    return mime_type_ret;
}

char *get_file_type(const char *file) {
    struct stat s;
    char *file_type = NULL;

    if(lstat(file, &s) == -1) {
        // printf("\x1b[31mERROR\x1b[0m: Can not read file type\n");
        return "Unknown";
    }

    switch (s.st_mode & S_IFMT) {
        case S_IFREG:
            file_type = "Regular File";
            break;
        case S_IFLNK:
            file_type = "Symbolic link";
            break;
        case S_IFCHR:
            file_type = "Character Device";
            break;
        case S_IFBLK:
            file_type = "Block Device";
            break;
        case S_IFIFO:
            file_type = "FIFO/Pipe";
            break;
        case S_IFSOCK:
            file_type = "Socket";
            break;
        default:
            file_type = "Unknown";
    }

    return file_type;
}

ino_t get_inode_number(const char *file) {
    struct stat s;

    if(lstat(file, &s) == -1) {
        // printf("\x1b[31mERROR\x1b[0m: Can not get inode number.\n");
        return (ino_t)0;
    }

    return s.st_ino;
}

nlink_t get_hard_link(const char *file) {
    struct stat s;

    if(lstat(file, &s) == -1) {
        // printf("\x1b[31mERROR\x1b[0m: Can not get number of hard link.\n");
        return (nlink_t)0;
    }

    return s.st_nlink;
}

unsigned long long get_filesize_bytes(const char *file) {
    struct stat s;

    if(lstat(file, &s) == -1) {
        // printf("\x1b[31mERROR\x1b[0m: Can not get file size.\n");
        return 0;
    }

    return (unsigned long long)s.st_size;
}

struct human_readable_size human_readable_size(const unsigned long long bytes) {
    double size = (double)bytes;
    int unit_index = 0;
    char *unit_table[] = { "B", "KB", "MB", "GB", "TB", "PB" };

    while(size >= 1024.0 && unit_index < 5) {
        size /= 1024.0;
        unit_index++;
    }

    return (struct human_readable_size){ size, unit_table[unit_index] };
}

struct block_allocator get_block_file(const char *file) {
    struct stat s;

    if(lstat(file, &s) == -1) {
        printf("\x1b[31mERROR\x1b[0m: Can not get block allocate.\n");
        return(struct block_allocator){ -1, -1 };
    }

    return (struct block_allocator){ (long long)s.st_blocks, (int32_t)s.st_blksize };
}

long long get_device_id(const char *file) {
    struct stat s;

    if(lstat(file, &s) == -1) {
        // printf("\x1b[31mERROR\x1b[0m: Can not get block allocate.\n");
        return -1;
    }

    return (long long)s.st_dev;
}

/*
 * Return 1 if sparse file, return 0 if not sparse file, return -1 if error when open file to read and detect
*/
int get_sparse_file(const char *file) {
    int fd = open(file, O_RDONLY);
    if(fd < 0) {
        // printf("\x1b[31mERROR\x1b[0m: Can not detect sparse file.\n");
        return -1;
    }

    struct stat s;
    if(lstat(file, &s) == -1) {
        // printf("\x1b[31mERROR\x1b[0m: Can not detect sparse file with stat.\n");
        return -1;
    }

    if(s.st_size == 0) {
        close(fd);
        return 0;
    }

    off_t off = 0;

	while(off < s.st_size) {
        off_t data = lseek(fd, off, SEEK_DATA);
        if (data == -1) {
            if (errno == ENXIO) {
                // From off to EOF always is hole
                close(fd);
                return 1;
            }
            // printf("\x1b[31mERROR\x1b[0m: error occur in lseek(SEEK_DATA).\n");
            close(fd);
            return -1;
        }

        off_t hole = lseek(fd, data, SEEK_HOLE);
        if (hole == -1) {
            // printf("\x1b[31mERROR\x1b[0m: error occur in lseek(SEEK_HOLE).\n");
            close(fd);
            return -1;
        }

        // If whitespace before data -> sparse
        if (data > off) {
            close(fd);
            return 1;
        }

        off = hole;
    }
    return 0;
}

struct owner_group get_owner_group_idname(const char *file) {
    struct stat s;
    struct owner_group owner_group;

    owner_group.group_id = 0;
    owner_group.owner_id = 0;

    owner_group.name_of_group = NULL;
    owner_group.name_of_owner = NULL;

    if(lstat(file, &s) == -1) {
        printf("\x1b[31mERROR\x1b[0m: Can not get MIME type. Recheck your file or system.\n");
        return owner_group;
    }

    owner_group.owner_id = s.st_uid;
    owner_group.group_id = s.st_gid;
    owner_group.name_of_owner = getpwuid(s.st_uid);
    owner_group.name_of_group = getgrgid(s.st_gid);

    return owner_group;
}


static char file_type_char(mode_t mode) {
    switch (mode & S_IFMT) {
        case S_IFREG:  return '-';
        case S_IFLNK:  return 'l';
        case S_IFCHR:  return 'c';
        case S_IFBLK:  return 'b';
        case S_IFIFO:  return 'p';
        case S_IFSOCK: return 's';
        default:       return '?';
    }
}

struct filemode get_access_mode_and_permission(const char *file) {
    struct stat s;
    struct filemode fmode;

    fmode.octal_access_mode = (mode_t)0;
    memset(fmode.symbolic_permission, 0x00, sizeof(fmode.symbolic_permission));

    if(lstat(file, &s) == -1) {
        // printf("\x1b[31mERROR\x1b[0m: Can not get access mode and file permission.\n");
        return fmode;
    }

    if(S_ISDIR(s.st_mode)) {
        return fmode;
    }

    char *sym = fmode.symbolic_permission;

    sym[0] = file_type_char(s.st_mode);

    /* user */
    sym[1] = (s.st_mode & S_IRUSR) ? 'r' : '-';
    sym[2] = (s.st_mode & S_IWUSR) ? 'w' : '-';
    sym[3] = (s.st_mode & S_IXUSR)
                ? ((s.st_mode & S_ISUID) ? 's' : 'x')
                : ((s.st_mode & S_ISUID) ? 'S' : '-');

    /* group */
    sym[4] = (s.st_mode & S_IRGRP) ? 'r' : '-';
    sym[5] = (s.st_mode & S_IWGRP) ? 'w' : '-';
    sym[6] = (s.st_mode & S_IXGRP)
                ? ((s.st_mode & S_ISGID) ? 's' : 'x')
                : ((s.st_mode & S_ISGID) ? 'S' : '-');

    /* other */
    sym[7] = (s.st_mode & S_IROTH) ? 'r' : '-';
    sym[8] = (s.st_mode & S_IWOTH) ? 'w' : '-';
    sym[9] = (s.st_mode & S_IXOTH)
                ? ((s.st_mode & S_ISVTX) ? 't' : 'x')
                : ((s.st_mode & S_ISVTX) ? 'T' : '-');

    sym[10] = '\0';

    fmode.octal_access_mode = s.st_mode & 07777;

    return fmode;
}

struct special_permision get_speccial_permission(const char *file) {
    struct stat s;
    struct special_permision spec_perm;

    spec_perm.sgid = 0;
    spec_perm.suid = 0;
    spec_perm.sticky_bits = 0;

    if(lstat(file, &s) == -1) {
        // printf("\x1b[31mERROR\x1b[0m: Can not get MIME type. Recheck your file or system.\n");
        return spec_perm;
    }

    spec_perm.suid        = s.st_mode & S_ISUID;
    spec_perm.sgid        = s.st_mode & S_ISGID;
    spec_perm.sticky_bits = s.st_mode & S_ISVTX;
    return spec_perm;
}

static char *format_time_alloc(time_t sec) {
    char tmp[64];
    struct tm tm;

    localtime_r(&sec, &tm);
    strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", &tm);

    return strdup(tmp);
}

void free_time_file(struct time_file_chrono *t) {
    free(t->born);
    free(t->access);
    free(t->modify);
    free(t->change);
}

struct time_file_chrono get_time_and_chrono_of_file(const char *file) {
    struct stat st;
    struct statx stx;
    struct time_file_chrono out;

    out.born  = NULL;
    out.access = NULL;
    out.modify = NULL;
    out.change = NULL;

    if (stat(file, &st) != 0)
        return out;

    if (statx(AT_FDCWD, file, AT_STATX_SYNC_AS_STAT,
              STATX_BTIME, &stx) == 0 &&
        (stx.stx_mask & STATX_BTIME)) {

        out.born = format_time_alloc(stx.stx_btime.tv_sec);
    } else {
        out.born = strdup("N/A");
    }

    /* atime / mtime / ctime */
    out.access = format_time_alloc(st.st_atim.tv_sec);
    out.modify = format_time_alloc(st.st_mtim.tv_sec);
    out.change = format_time_alloc(st.st_ctim.tv_sec);

    return out;
}

// * NOTE: 4 function below is empty. Continue to write
void BasicInformation(const char* file) {
    struct BASIC_INFORMATION basic_information;

    basic_information.title = "[ BASIC INFORMATION ]";
    basic_information.filename = strdup(file);
    basic_information.type.filetype = get_file_type(file);
    basic_information.type.mimetype = MIME_file_type(file);
    realpath(file, basic_information.actualpath);

    // Print metadata of basic information
    printf("%s\n", basic_information.title);
    printf("  %-15s : %s\n", "File Name", basic_information.filename);
    printf("  %-15s : %s (MIME: %s)\n", "File Type", basic_information.type.filetype, basic_information.type.mimetype);
    printf("  %-15s : %s\n\n", "Location", basic_information.actualpath);

    // free memory allocated by strdup()
    free(basic_information.filename);
    free(basic_information.type.mimetype);
}

void StorageDetails(const char* file) {
    struct STORAGE_DETAILS storage_details;

    storage_details.title = "[ STORAGE DETAILS ]";

    storage_details.inode_number = get_inode_number(file);
    storage_details.hardlink_number = get_hard_link(file);
    storage_details.size.bytes_size = get_filesize_bytes(file);
    storage_details.size.hrd_size = human_readable_size(storage_details.size.bytes_size);
    storage_details.blk_allocator = get_block_file(file);
    storage_details.device_id = get_device_id(file);
    storage_details.sparse_file = get_sparse_file(file);

    // Print metadata of storage details
    printf("%s\n", storage_details.title);
    printf("  %-15s : %llu\n", "Inode Number", (unsigned long long)storage_details.inode_number);
    printf("  %-15s : %llu\n", "Links (Hard)", (unsigned long long)storage_details.hardlink_number);
    printf("  %-15s : %" PRIu64 " bytes (%f %s)\n", "Total size", storage_details.size.bytes_size, storage_details.size.hrd_size.value, storage_details.size.hrd_size.unit);
    printf("  %-15s : %" PRId64 "\n", "Block Allocated", storage_details.blk_allocator.blk_allocate);
    printf("  %-15s : %" PRId32 " bytes\n", "IO Block Size", storage_details.blk_allocator.io_blk_size);
    printf("  %-15s : %llxh / %lld\n", "Device ID", storage_details.device_id, storage_details.device_id);
    printf("  %-15s : %s\n\n", "Sparse File", ((storage_details.sparse_file > 0)? "Yes" : "No"));
}

void SecurityOwnership(const char* file) {
    struct SECURITY_OWNERSHIP security_ownership;

    memset(security_ownership.fmode.symbolic_permission, 0x00, sizeof(security_ownership.fmode.symbolic_permission));

    security_ownership.title = "[ SECURITY & OWNERSHIP ]";
    security_ownership.fmode = get_access_mode_and_permission(file);
    security_ownership.own_grp = get_owner_group_idname(file);
    security_ownership.spec_perm = get_speccial_permission(file);

    // Print metadata of security ownerships
    printf("%s\n", security_ownership.title);
    printf("  %-15s : %04u%*s%s: %s\n", "Access Mode", security_ownership.fmode.octal_access_mode, 15, "", "Symbolic", security_ownership.fmode.symbolic_permission);
    printf("  %-15s : %u%*s%s: %s\n", "Owner (UID)", security_ownership.own_grp.owner_id, 15, "", "Name", security_ownership.own_grp.name_of_owner->pw_name);
    printf("  %-15s : %u%*s%s: %s\n", "Group (GID)", security_ownership.own_grp.group_id, 15, "", "Name", security_ownership.own_grp.name_of_group->gr_name);
    if(security_ownership.spec_perm.suid != 0 && security_ownership.spec_perm.sgid != 0) {
        (security_ownership.spec_perm.sticky_bits == 0)?
            printf("  %-15s : %04o/%04o%*s%s: %s\n", "SUID/SGID", security_ownership.spec_perm.suid, security_ownership.spec_perm.sgid, 15, "", "Sticky Bit", "Not set")
            :
            printf("  %-15s : %04o/%04o%*s%s: %04o\n", "SUID/SGID", security_ownership.spec_perm.suid, security_ownership.spec_perm.sgid, 15, "", "Sticky Bit", security_ownership.spec_perm.sticky_bits);
    }
    else if((security_ownership.spec_perm.suid == 0 && security_ownership.spec_perm.sgid != 0) || (security_ownership.spec_perm.suid != 0 && security_ownership.spec_perm.sgid == 0)) {
        if(security_ownership.spec_perm.suid == 0) {
            (security_ownership.spec_perm.sticky_bits == 0)?
                printf("  %-15s : %s/%04o%*s%s: %s\n", "SUID/SGID", "Not set", security_ownership.spec_perm.sgid, 15, "", "Sticky Bit", "Not set")
                :
                printf("  %-15s : %s/%04o%*s%s: %04o\n", "SUID/SGID", "Not set", security_ownership.spec_perm.sgid, 15, "", "Sticky Bit", security_ownership.spec_perm.sticky_bits);
        }
        else if(security_ownership.spec_perm.sgid == 0) {
            (security_ownership.spec_perm.sticky_bits == 0)?
                printf("  %-15s : %04o/%s%*s%s: %s\n", "SUID/SGID", security_ownership.spec_perm.suid, "Not set", 15, "", "Sticky Bit", "Not set")
                :
                printf("  %-15s : %04o/%s%*s%s: %04o\n", "SUID/SGID", security_ownership.spec_perm.suid, "Not set", 15, "", "Sticky Bit", security_ownership.spec_perm.sticky_bits);
        }
    }
    else {
        printf("  %-15s : %s%*s%s: %s\n", "SUID/SGID", "Not set", 15, "", "Sticky Bit", "Not set");
    }
    printf("\n");
}

void TimeChronology(const char* file) {
    struct TIME_CHORONOLOGY timefile;

    timefile.title = "[ TIME CHRONOLOGY ]";
    timefile.time_file = get_time_and_chrono_of_file(file);

    // Print metadata of time chronology
    printf("%s\n", timefile.title);
    printf("  %-15s : %s ", "Born (Birth)", timefile.time_file.born);
    if(strcmp(timefile.time_file.born, "N/A") == 0)
        printf("N/A on some Filesystems");
    printf("\n");

    printf("  %-15s : %s (Read/Open)\n", "Last Access", timefile.time_file.access);
    printf("  %-15s : %s (Content change)\n", "Last Modify", timefile.time_file.modify);
    printf("  %-15s : %s (Metadata change)\n\n", "Last Change", timefile.time_file.change);


    free_time_file(&timefile.time_file);
}

void SystemFlags(const char* file) {
    int fd = open(file, O_RDONLY);
    if(fd == -1) {
        printf("\x1b[31mERROR\x1b[0m: Cannot open file.\n");
        exit(1);
    }

    int flags;
    if(ioctl(fd, FS_IOC_GETFLAGS, &flags) == -1) {
        printf("\x1b[31mERROR\x1b[0m: Cannot get flags from file.\n");
        close(fd);
        exit(1);
    }

    printf("[ SYSTEM FLAGS ]\n");
    printf("  %-15s : %s\n", "Immutable", (flags & FS_IMMUTABLE_FL) ? "Yes" : "No");
    printf("  %-15s : %s\n", "Append Only", (flags & FS_APPEND_FL) ? "Yes" : "No");

    close(fd);
}

void PrintingMetadata(struct FILEMETADATA FileMetadata) {
    for(uint32_t index = 0; index < FileMetadata.count_file; index++) {
        printf("======================= FILE METADATA EXTRACTOR =======================\n");
        BasicInformation(FileMetadata.filename[index]);
        StorageDetails(FileMetadata.filename[index]);
        SecurityOwnership(FileMetadata.filename[index]);
        TimeChronology(FileMetadata.filename[index]);
        SystemFlags(FileMetadata.filename[index]);
        printf("=======================================================================\n");
        printf("\n"); // Newline to extract next file
    }
}


// ### Main ### //
int main(int argc, char**argv) {
    // Argument checking
    if(argc < 2) {
        printf("\x1b[31mERROR\x1b[0m: Not enough argument. Must be at least 1 file input, version checking or help\n");
        return 1;
    }

    if(strcmp(argv[1], "-h") == 0) {
        Usage();
        return 0;
    }
    if(strcmp(argv[1] , "-v") == 0) {
        printf("cfile version: 0.2.2#dev\n");
        return 0;
    }

    // Initalize all data
    struct FILEMETADATA FileMetadata;
    FileMetadata.count_file = 0;
    FileMetadata.cap_file = 2;
    FileMetadata.filename = malloc(FileMetadata.cap_file * sizeof(char));
    if(!FileMetadata.filename) {
        printf("\x1b[31mERROR\x1b[0m: Memory allocation failed at main\n");
        return 1;
    }

    int retParse = ArgParse(&FileMetadata, argc, argv);
    if(retParse != 0) {
        MemFree(&FileMetadata);
        exit(1);
    }

    if(FileMetadata.count_file == 0) {
        printf("\x1b[31mERROR\x1b[0m: Must be at least 1 file input\n");
        MemFree(&FileMetadata);
        exit(1);
    }

    // Starting extract file metadata;
    PrintingMetadata(FileMetadata);
    MemFree(&FileMetadata);

    // Return 0
    return 0;
}

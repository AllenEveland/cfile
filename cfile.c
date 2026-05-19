#define _GNU_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <linux/fs.h>
#include <magic.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    // File
    int count_file;
    int cap_file;
    char **filename;
} FILEMETADATA;

struct humanread_size {
    double value;
    const char *unit;
};

struct block_alloc {
    const long long block_allocate;
    const long io_block_size;
};

struct file_mode {
    mode_t octal_access_mode;
    char symbolic_permission[11];
};

struct own_grp {
    uid_t owner_id;
    gid_t group_id;

    struct passwd *name_of_owner;
    struct group *name_of_group;
};

struct spec_perm {
    mode_t suid;
    mode_t sgid;
    mode_t sticky_bits;
};

struct time_file_chrono {
    char *born;
    char *access;
    char *modify;
    char *change;
};

void Usage() {
    printf("Usage: cfile [file1, file2, ...]\n");
}

int check_file_valid(char *filename) {
    if (filename == NULL) {
        printf("\x1b[31mERROR\x1b[0m: Non file checked.\n");
        return 0;
    }

    // --- Check file exist and readable
    if (access(filename, F_OK) != 0) {
        printf("\x1b[31mERROR\x1b[0m: Can not access file to read.\n");
        return 0;
    }
    return 1;
}

char *MIME_File_Type(const char* file) {
    magic_t magic_file_checking;
    const char *mime_type = NULL;
    magic_file_checking = magic_open(MAGIC_MIME_TYPE);

    // Checking magic initialization
    if(magic_file_checking == NULL) {
        printf("\x1b[31mERROR\x1b[0m: Can not initialize magic library\n");
        return NULL;
    }

    // Loading operating system database
    if(magic_load(magic_file_checking, NULL) != 0) {
        printf("\x1b[31mERROR\x1b[0m: Can not load database");
        magic_close(magic_file_checking);
        return NULL;
    }

    // Reading type of file
    mime_type = magic_file(magic_file_checking, file);
    if(mime_type == NULL) {
        printf("\x1b[31mERROR\x1b[0m: %s\n", magic_error(magic_file_checking));
    }

    char *mime_type_ret = strdup(mime_type);
    magic_close(magic_file_checking);

    return mime_type_ret;
}

char *get_file_type(const char *file) {
    struct stat s;
    char *file_type = NULL;

    if(lstat(file, &s) == -1) {
        printf("\x1b[31mERROR\x1b[0m: Can not read file type\n");
        return NULL;
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
        printf("\x1b[31mERROR\x1b[0m: Can not get inode number.\n");
        return (ino_t)0;
    }

    return s.st_ino;
}

nlink_t get_hard_link(const char *file) {
    struct stat s;
    
    if(lstat(file, &s) == -1) {
        printf("\x1b[31mERROR\x1b[0m: Can not get number of hard link.\n");
        return (nlink_t)0;
    }

    return s.st_nlink;
}

unsigned long long get_filesize_bytes(const char *file) {
    struct stat s;

    if(lstat(file, &s) == -1) {
        printf("\x1b[31mERROR\x1b[0m: Can not get file size.\n");
        return 0;
    }

    return (unsigned long long)s.st_size;
}

struct humanread_size human_readable_size(const unsigned long long bytes) {
    double size = (double)bytes;
    int unit_index = 0;
    char *unit_table[] = { "B", "KB", "MB", "GB", "TB", "PB" };

    while(size >= 1024.0 && unit_index < 5) {
        size /= 1024.0;
        unit_index++;
    }

    return (struct humanread_size){ size, unit_table[unit_index] };
}

struct block_alloc get_block_file(const char *file) {
    struct stat s;

    if(lstat(file, &s) == -1) {
        printf("\x1b[31mERROR\x1b[0m: Can not get block allocate.\n");
        return(struct block_alloc){ -1, -1 };
    }

    return (struct block_alloc){ (long long)s.st_blocks, (long)s.st_blksize };
}

long long get_device_id(const char *file) {
    struct stat s;

    if(lstat(file, &s) == -1) {
        printf("\x1b[31mERROR\x1b[0m: Can not get block allocate.\n");
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
        printf("\x1b[31mERROR\x1b[0m: Can not detect sparse file.\n");
        return -1;
    }

    struct stat s;
    if(lstat(file, &s) == -1) {
        printf("\x1b[31mERROR\x1b[0m: Can not detect sparse file with stat.\n");
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
            printf("\x1b[31mERROR\x1b[0m: error occur in lseek(SEEK_DATA).\n");
            close(fd);
            return -1;
        }

        off_t hole = lseek(fd, data, SEEK_HOLE);
        if (hole == -1) {
            printf("\x1b[31mERROR\x1b[0m: error occur in lseek(SEEK_HOLE).\n");
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

int get_access_mode_and_permission(const char *file, struct file_mode *fmode) {
    struct stat s;

    if(lstat(file, &s) == -1) {
        printf("\x1b[31mERROR\x1b[0m: Can not get access mode and file permission.\n");
        return 0;
    }

    if(S_ISDIR(s.st_mode)) {
        return 0;
    }

    char *sym = fmode->symbolic_permission;

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

    fmode->octal_access_mode = s.st_mode & 07777;
    return 1;
}

int get_owner_group_idname(const char *file, struct own_grp *owner_group) {
    struct stat s;

    if(lstat(file, &s) == -1) {
        printf("\x1b[31mERROR\x1b[0m: Can not get MIME type. Recheck your file or system.\n");
        return 0;
    }

    owner_group->owner_id = s.st_uid;
    owner_group->group_id = s.st_gid;
    owner_group->name_of_owner = getpwuid(s.st_uid);
    owner_group->name_of_group = getgrgid(s.st_gid);
    return 1;
}

int get_spec_permission(const char *file, struct spec_perm *special_permission) {
    struct stat s;
    
    if(lstat(file, &s) == -1) {
        printf("\x1b[31mERROR\x1b[0m: Can not get MIME type. Recheck your file or system.\n");
        return 0;
    }
    special_permission->suid        = s.st_mode & S_ISUID;
    special_permission->sgid        = s.st_mode & S_ISGID;
    special_permission->sticky_bits = s.st_mode & S_ISVTX;
    return 1;
}

static char *format_time_alloc(time_t sec) {
    char tmp[64];
    struct tm tm;

    localtime_r(&sec, &tm);
    strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", &tm);

    return strdup(tmp);
}

void free_file_times(struct time_file_chrono *t) {
    free(t->born);
    free(t->access);
    free(t->modify);
    free(t->change);
}

int get_time_and_chrono_of_file(const char *file, struct time_file_chrono *out) {
    struct stat st;
    struct statx stx;

    out->born  = NULL;
    out->access = NULL;
    out->modify = NULL;
    out->change = NULL;

    if (stat(file, &st) != 0)
        return 0;

    if (statx(AT_FDCWD, file, AT_STATX_SYNC_AS_STAT,
              STATX_BTIME, &stx) == 0 &&
        (stx.stx_mask & STATX_BTIME)) {

        out->born = format_time_alloc(stx.stx_btime.tv_sec);
    } else {
        out->born = strdup("N/A");
    }

    /* atime / mtime / ctime */
    out->access = format_time_alloc(st.st_atim.tv_sec);
    out->modify = format_time_alloc(st.st_mtim.tv_sec);
    out->change = format_time_alloc(st.st_ctim.tv_sec);

    return 1;
}

int Basic_Information(const char *file) {
    char *mime_type = MIME_File_Type(file);
    char *file_type = get_file_type(file);
    char actual_path[PATH_MAX];

    if(mime_type == NULL) {
        printf("\x1b[31mERROR\x1b[0m: Can not get MIME type. Recheck your file or system.\n");
        return 0;
    }
    if(file_type == NULL) {
        printf("\x1b[31mERROR\x1b[0m: Can not get file type. Recheck your file or system.\n");
        return 0;
    }

    if(realpath(file, actual_path) == NULL) {
        printf("\x1b[31mERROR\x1b[0m: Can not get directory of file. Recheck your file or system.\n");
        return 0;
    }
    printf("[ BASIC INFORMATION ]\n");
    printf("  %-15s : %s\n", "File Name", file);
    printf("  %-15s : %s (MIME: %s)\n", "File Type", file_type, mime_type);
    printf("  %-15s : %s\n\n", "Location", actual_path);
    return 1;
}

int Storage_Details(const char *file) {
    ino_t inode_number = get_inode_number(file);
    nlink_t hardlink_number = get_hard_link(file);

    unsigned long long bytes_size = get_filesize_bytes(file);
    struct humanread_size hrd_size = human_readable_size(bytes_size);
    struct block_alloc blkalloc = get_block_file(file);
    long long device_id = get_device_id(file);
    int sparse_file = get_sparse_file(file);
    
    if(inode_number == 0) return 0;
    if(hardlink_number == 0) return 0;
    if(bytes_size ==0) return 0;
    if(blkalloc.block_allocate < 0 || blkalloc.io_block_size < 0) return 0;
    if(device_id == -1) return 0;
    if(sparse_file < 0) return 0;

    printf("[ STORAGE DETAILS ]\n");
    printf("  %-15s : %llu\n", "Inode Number", (unsigned long long)inode_number);
    printf("  %-15s : %llu\n", "Links (Hard)", (unsigned long long)hardlink_number);
    printf("  %-15s : %llu bytes (%f %s)\n", "Total size", bytes_size, hrd_size.value, hrd_size.unit);
    printf("  %-15s : %lld\n", "Block Allocated", blkalloc.block_allocate);
    printf("  %-15s : %ld bytes\n", "IO Block Size", blkalloc.io_block_size);
    printf("  %-15s : %llxh / %lld\n", "Device ID", device_id, device_id);
    printf("  %-15s : %s\n\n", "Sparse File", ((sparse_file > 0)? "Yes" : "No"));
    return 1;
}

int Security_Ownership(const char *file) {
    struct file_mode fmode;
    struct own_grp owner_group;
    struct spec_perm special_permission;
    memset(fmode.symbolic_permission, 0x00, sizeof(fmode.symbolic_permission));
    if(get_access_mode_and_permission(file, &fmode) == 0) return 0;
    if(get_owner_group_idname(file, &owner_group) == 0) return 0;
    if(get_spec_permission(file, &special_permission) == 0) return 0;

    printf("[ SECURITY & OWNERSHIP ]\n");
    printf("  %-15s : %04u%*s%s: %s\n", "Access Mode", fmode.octal_access_mode, 15, "", "Symbolic", fmode.symbolic_permission);
    printf("  %-15s : %u%*s%s: %s\n", "Owner (UID)", owner_group.owner_id, 15, "", "Name", owner_group.name_of_owner->pw_name);
    printf("  %-15s : %u%*s%s: %s\n", "Group (GID)", owner_group.group_id, 15, "", "Name", owner_group.name_of_group->gr_name);
    if(special_permission.suid != 0 && special_permission.sgid != 0) {
        (special_permission.sticky_bits == 0)?
            printf("  %-15s : %04o/%04o%*s%s: %s\n", "SUID/SGID", special_permission.suid, special_permission.sgid, 15, "", "Sticky Bit", "Not set")
            :
            printf("  %-15s : %04o/%04o%*s%s: %04o\n", "SUID/SGID", special_permission.suid, special_permission.sgid, 15, "", "Sticky Bit", special_permission.sticky_bits);
    }
    else if((special_permission.suid == 0 && special_permission.sgid != 0) || (special_permission.suid != 0 && special_permission.sgid == 0)) {
        if(special_permission.suid == 0) {
            (special_permission.sticky_bits == 0)?
                printf("  %-15s : %s/%04o%*s%s: %s\n", "SUID/SGID", "Not set", special_permission.sgid, 15, "", "Sticky Bit", "Not set")
                :
                printf("  %-15s : %s/%04o%*s%s: %04o\n", "SUID/SGID", "Not set", special_permission.sgid, 15, "", "Sticky Bit", special_permission.sticky_bits);
        }
        else if(special_permission.sgid == 0) {
            (special_permission.sticky_bits == 0)?
                printf("  %-15s : %04o/%s%*s%s: %s\n", "SUID/SGID", special_permission.suid, "Not set", 15, "", "Sticky Bit", "Not set")
                :
                printf("  %-15s : %04o/%s%*s%s: %04o\n", "SUID/SGID", special_permission.suid, "Not set", 15, "", "Sticky Bit", special_permission.sticky_bits);
        }
    }
    else {
        printf("  %-15s : %s%*s%s: %s\n", "SUID/SGID", "Not set", 15, "", "Sticky Bit", "Not set");
    }
    printf("\n");
    return 1;
}

int Time_Chronology(const char *file) {
    struct time_file_chrono time_file;
    if(get_time_and_chrono_of_file(file, &time_file) == 0) return 0;

    printf("[ TIME CHRONOLOGY ]\n");
    printf("  %-15s : %s ", "Born (Birth)", time_file.born);
    if(strcmp(time_file.born, "N/A") == 0)
        printf("N/A on some Filesystems");
    printf("\n");

    printf("  %-15s : %s (Read/Open)\n", "Last Access", time_file.access);
    printf("  %-15s : %s (Content change)\n", "Last Modify", time_file.modify);
    printf("  %-15s : %s (Metadata change)\n\n", "Last Change", time_file.change);

    free_file_times(&time_file);
    return 1;
}

int System_Flags(const char *file) {
    int fd = open(file, O_RDONLY);
    if(fd == -1) {
        printf("\x1b[31mERROR\x1b[0m: Cannot open file.\n");
        return 0;
    }

    int flags;
    if(ioctl(fd, FS_IOC_GETFLAGS, &flags) == -1) {
        printf("\x1b[31mERROR\x1b[0m: Cannot get flags from file.\n");
        close(fd);
        return 0;
    }

    printf("[ SYSTEM FLAGS ]\n");
    printf("  %-15s : %s\n", "Immutable", (flags & FS_IMMUTABLE_FL) ? "Yes" : "No");
    printf("  %-15s : %s\n", "Append Only", (flags & FS_APPEND_FL) ? "Yes" : "No");

    return 1;
}

int ArgParse(FILEMETADATA *FileMetadata, int argc, char *argv[]) {
    for(int tok=1; tok<argc; tok++) {
        int check_valid_file_retnum = check_file_valid(argv[tok]);
        if(check_valid_file_retnum == 0) return 0;
        else if(check_valid_file_retnum) {
            if(FileMetadata->count_file == FileMetadata->cap_file) {
                int new_cap = FileMetadata->cap_file? FileMetadata->cap_file+1 : 1;
                char **tmp = realloc(FileMetadata->filename, new_cap*sizeof(char*));
                if(!tmp) {
                    printf("\x1b[31mERROR\x1b[0m: Cannot reallocate memory for saving file. May be recheck your system.\n");
                    return 0;
                }
                FileMetadata->filename = tmp;
                FileMetadata->cap_file = new_cap;
            }
            FileMetadata->filename[FileMetadata->count_file++] = argv[tok];
        }
        else {
            printf("\x1b[31mERROR\x1b[0m: Unknown flags or value: %s\n", argv[tok]);
            return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[]) {
    // Argument checking
    if(argc < 2) {
        printf("\x1b[31mERROR\x1b[0m: Not enough argument. Must be at least 1 file input, version checking or help\n");
        exit(1);
    }

    if(strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        Usage();
        return 0;
    }
    if(strcmp(argv[1], "--version") == 0 || strcmp(argv[1] , "-v") == 0 || strcmp(argv[1], "-V") == 0) {
        printf("cfile version: 0.1#dev\n");
        return 0;
    }

    // Initalize all data
    FILEMETADATA *FileMetadata = calloc(1, sizeof(*FileMetadata));

    int retParse = ArgParse(FileMetadata, argc, argv);
    if(retParse != 1) {
        free(FileMetadata->filename);
        exit(1);
    }

    if(FileMetadata->filename == NULL) {
        printf("\x1b[31mERROR\x1b[0m: Must be at least 1 file input\n");
        free(FileMetadata->filename);
        exit(1);
    }

    /*
     * NOTE: Information of file handling in here
    */

    for(int i=0; i<FileMetadata->count_file; i++) {
        printf("======================= FILE METADATA EXTRACTOR =======================\n");
        // BASIC INFORMATION
        if(Basic_Information(FileMetadata->filename[i]) != 1) exit(1);
        if(Storage_Details(FileMetadata->filename[i]) != 1) exit(1);
        if(Security_Ownership(FileMetadata->filename[i]) != 1) exit(1);
        if(Time_Chronology(FileMetadata->filename[i]) != 1) exit(1);
        if(System_Flags(FileMetadata->filename[i]) != 1) exit(1);
        printf("=======================================================================\n");
    }

    /*
     * NOTE: End of file information checking 
    */

    free(FileMetadata->filename);
    return 0;
}


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fuse.h>
#include "log.h"

FILE *open_log_file(void)
{
    static FILE *log_file;
    if (!log_file)
    {
        log_file = fopen(set_log_name(NULL), "w");
        if (!log_file)
        {
            fprintf(stderr, "open log failed!\n");
            exit(EXIT_FAILURE);
        }
        setvbuf(log_file, NULL, _IOLBF, 0);
    }

    return log_file;
}

const char *set_log_name(const char *log_name)
{
    static char name[512] = "disk.log";
    if (log_name)
    {
        strncpy(name, log_name, 511);
        name[511] = '\0';
    }
    return name;
}

void log_msg(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(open_log_file(), format, ap);
    va_end(ap);
}

void log_fi (const struct fuse_file_info *fi)
{
    log_msg("    fi:\n");
	log_struct("    ", fi, fh, "0x%016llx");
}

void log_stat(const struct stat *si)
{
    log_msg("    si:\n");
	log_struct("    ", si, st_dev, "%lu");
	log_struct("    ", si, st_ino, "%lu");
	log_struct("    ", si, st_mode, "0%o");
	log_struct("    ", si, st_nlink, "%lu");
	log_struct("    ", si, st_uid, "%u");
	log_struct("    ", si, st_gid, "%u");
	log_struct("    ", si, st_rdev, "%lu");
	log_struct("    ", si, st_size, "%ld");
	log_struct("    ", si, st_blksize, "%ld");
	log_struct("    ", si, st_blocks, "%ld");
	log_struct("    ", si, st_atime, "0x%08lx");
	log_struct("    ", si, st_mtime, "0x%08lx");
	log_struct("    ", si, st_ctime, "0x%08lx");
}

void log_statvfs(const struct statvfs *sv)
{
    log_msg("    sv:\n");
	log_struct("indent", sv, f_bsize, "%ld");
	log_struct("indent", sv, f_frsize, "%ld");
	log_struct("indent", sv, f_blocks, "%lld");
	log_struct("indent", sv, f_bfree, "%lld");
	log_struct("indent", sv, f_bavail, "%lld");
	log_struct("indent", sv, f_files, "%lld");
	log_struct("indent", sv, f_ffree, "%lld");
	log_struct("indent", sv, f_favail, "%lld");
	log_struct("indent", sv, f_fsid, "%ld");
	log_struct("indent", sv, f_flag, "0x%08lx");
	log_struct("indent", sv, f_namemax, "%ld");
}
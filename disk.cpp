#define FUSE_USE_VERSION 30
#include <vector>
#include <string>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <errno.h>
#include "dev_io.h"

fat32::path parse_path(const char *path);
// {
//     std::vector<std::wstring> res;
//     wchar_t buf[256];
//     int index = 0;
//     while (*FileName != L'\0' && *FileName != L':')
//     {
//         if (*FileName == L'\\')
//         {
//             index = 0;
//             memset(buf, 0, sizeof(buf));
//         }
//         else
//         {
//             buf[index++] = *FileName;
//             if (FileName[1] == L'\\' || FileName[1] == L'\0')
//             {
//                 res.emplace_back(buf);
//             }
//         }
//         ++FileName;
//     }
//     return res;
// }

std::string wide2local(std::wstring_view str)
{
    return std::string();
}

dev_io::dev_t &get_dev()
{
    static dev_io::dev_t dev("test.img");
    if (!dev)
    {
        exit(EXIT_FAILURE);
    }
    return dev;
}

static int vfat_getattr(const char *path, struct stat *stbuf)
{
    return get_dev().stat(parse_path(path), stbuf);
}

static int vfat_access(const char *path, int mask)
{
    if (mask == F_OK)
    {
        return get_dev().access(parse_path(path));
    }
    return 0;
}

static int vfat_readlink(const char *path, char *buf, size_t size)
{
    //future to implement
    return 0;
}

static int vfat_opendir(const char *path, struct fuse_file_info *fi)
{
    return get_dev().opendir(parse_path(path), &fi->fh);
}

static int vfat_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t, struct fuse_file_info *fi)
{
    fat32::dir_info entries;
    auto ret = get_dev().readdir(fi->fh, entries);
    for (const auto &entry : entries)
    {
        if (filler(buf, wide2local(entry.name).c_str(), &entry.info, 0))
            break;
    }
    return ret;
}

static int vfat_releasedir(const char *, struct fuse_file_info *fi)
{
    get_dev().closedir(fi->fh);
    return 0;
}

static int vfat_mknod(const char *path, mode_t mode, dev_t rdev)
{
    return get_dev().mknod(parse_path(path), mode);
}

static int vfat_mkdir(const char *path, mode_t mode)
{
    return get_dev().mkdir(parse_path(path), mode);
}

static int vfat_unlink(const char *path)
{
    return get_dev().unlink(parse_path(path));
}

static int vfat_rmdir(const char *path)
{
    return get_dev().rmdir(parse_path(path));
}

static int vfat_symlink(const char *from, const char *to)
{
    //future to implement
    return 0;
}

static int vfat_rename(const char *from, const char *to)
{
    return get_dev().rename(parse_path(from), parse_path(to));
}

static int vfat_link(const char *from, const char *to)
{
    //future implement;
    return 0;
}

static int vfat_chmod(const char *path, mode_t mode)
{
    return 0;
}

static int vfat_chown(const char *path, uid_t uid, gid_t gid)
{
    return 0;
}

static int vfat_truncate(const char *path, off_t size)
{
    return get_dev().truncate(parse_path(path), size);
}

static int vfat_open(const char *path, struct fuse_file_info *fi)
{
    return get_dev().open(parse_path(path), fi->flags, &fi->fh);
}

static int vfat_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    return get_dev().read(fi->fh, offset, size, buf);
}

static int vfat_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
{
    return get_dev().write(fi->fh, offset, size, buf);
}

static int vfat_statfs(const char *path, struct statvfs *stbuf)
{
    return get_dev().statfs(stbuf);
}

static int vfat_utimens(const char *path, const struct timespec tv[2])
{
    return get_dev().utimens(parse_path(path), tv);
}

static int vfat_release(const char *, struct fuse_file_info *fi)
{
    get_dev().close(fi->fh);
    return 0;
}

static int vfat_fsync(const char *, int, struct fuse_file_info *)
{
    get_dev().flush();
    return 0;
}

static struct fuse_operations operation;

int main(int argc, char *argv[])
{
    memset(&operation, 0, sizeof(operation));
    operation.getattr = vfat_getattr;
    operation.access = vfat_access;
    operation.readlink = vfat_readlink;
    operation.opendir = vfat_opendir;
    operation.readdir = vfat_readdir;
    operation.releasedir = vfat_releasedir;
    operation.mknod = vfat_mknod;
    operation.mkdir = vfat_mkdir;
    operation.symlink = vfat_symlink;
    operation.unlink = vfat_unlink;
    operation.rmdir = vfat_rmdir;
    operation.rename = vfat_rename;
    operation.link = vfat_link;
    operation.chmod = vfat_chmod;
    operation.chown = vfat_chown;
    operation.truncate = vfat_truncate;
    operation.open = vfat_open;
    operation.read = vfat_read;
    operation.write = vfat_write;
    operation.statfs = vfat_statfs;
    operation.utimens = vfat_utimens;
    operation.release = vfat_release;
    operation.fsync = vfat_fsync;
    return fuse_main(argc, argv, &operation, NULL);
}
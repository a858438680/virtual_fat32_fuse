#define FUSE_USE_VERSION 30
#include <vector>
#include <string>
#include <mutex>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <iconv.h>
#include <errno.h>
#include "dev_io.h"
#include "log.h"
#include "iconvpp.hpp"

std::mutex global_mtx;

std::string wide2local(std::u16string_view str)
{
    iconvpp::converter cnvt("UTF-8", "UTF-16LE", true);
    return cnvt.convert(std::string_view((const char *)str.data(), str.length() * sizeof(char16_t)));
}

std::u16string local2wide(std::string_view str)
{
    iconvpp::converter cnvt("UTF-16LE", "UTF-8", true);
    auto tmp = cnvt.convert(str);
    return std::u16string((const char16_t *)tmp.data(), tmp.length() / sizeof(char16_t));
}

fat32::path parse_path(std::string_view path)
{
    fat32::path res;
    std::string buf;
    for (auto c : path)
    {
        if (c == '/')
        {
            buf.clear();
        }
        else
        {
            buf.push_back(c);
            if (path[1] == '/')
            {
                res.emplace_back(local2wide(buf));
            }
        }
    }
    if (!buf.empty())
        res.emplace_back(local2wide(buf));
    return res;
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
    std::lock_guard<std::mutex> g(global_mtx);
    return get_dev().stat(parse_path(path), stbuf);
}

static int vfat_access(const char *path, int mask)
{
    std::lock_guard<std::mutex> g(global_mtx);
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
    std::lock_guard<std::mutex> g(global_mtx);
    return get_dev().open(parse_path(path), &fi->fh);
}

static int vfat_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t, struct fuse_file_info *fi)
{
    std::lock_guard<std::mutex> g(global_mtx);
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
    std::lock_guard<std::mutex> g(global_mtx);
    get_dev().close(fi->fh);
    return 0;
}

static int vfat_mknod(const char *path, mode_t mode, dev_t rdev)
{
    std::lock_guard<std::mutex> g(global_mtx);
    return get_dev().mknod(parse_path(path));
}

static int vfat_mkdir(const char *path, mode_t mode)
{
    std::lock_guard<std::mutex> g(global_mtx);
    return get_dev().mkdir(parse_path(path));
}

static int vfat_unlink(const char *path)
{
    std::lock_guard<std::mutex> g(global_mtx);
    return get_dev().unlink(parse_path(path));
}

static int vfat_rmdir(const char *path)
{
    std::lock_guard<std::mutex> g(global_mtx);
    return get_dev().rmdir(parse_path(path));
}

static int vfat_symlink(const char *from, const char *to)
{
    std::lock_guard<std::mutex> g(global_mtx);
    //future to implement
    return 0;
}

static int vfat_rename(const char *from, const char *to)
{
    std::lock_guard<std::mutex> g(global_mtx);
    return get_dev().rename(parse_path(from), parse_path(to));
}

static int vfat_link(const char *from, const char *to)
{
    std::lock_guard<std::mutex> g(global_mtx);
    //future implement;
    return 0;
}

static int vfat_chmod(const char *path, mode_t mode)
{
    std::lock_guard<std::mutex> g(global_mtx);
    return 0;
}

static int vfat_chown(const char *path, uid_t uid, gid_t gid)
{
    std::lock_guard<std::mutex> g(global_mtx);
    return 0;
}

static int vfat_truncate(const char *path, off_t size)
{
    std::lock_guard<std::mutex> g(global_mtx);
    return get_dev().truncate(parse_path(path), size);
}

static int vfat_open(const char *path, struct fuse_file_info *fi)
{
    std::lock_guard<std::mutex> g(global_mtx);
    return get_dev().open(parse_path(path), &fi->fh);
}

static int vfat_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    std::lock_guard<std::mutex> g(global_mtx);
    return get_dev().read(fi->fh, offset, size, buf);
}

static int vfat_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
{
    std::lock_guard<std::mutex> g(global_mtx);
    return get_dev().write(fi->fh, offset, size, buf);
}

static int vfat_statfs(const char *path, struct statvfs *stbuf)
{
    std::lock_guard<std::mutex> g(global_mtx);
    return get_dev().statfs(stbuf);
}

static int vfat_utimens(const char *path, const struct timespec tv[2])
{
    std::lock_guard<std::mutex> g(global_mtx);
    return get_dev().utimens(parse_path(path), tv);
}

static int vfat_release(const char *, struct fuse_file_info *fi)
{
    std::lock_guard<std::mutex> g(global_mtx);
    get_dev().close(fi->fh);
    return 0;
}

static int vfat_fsync(const char *, int, struct fuse_file_info *)
{
    std::lock_guard<std::mutex> g(global_mtx);
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
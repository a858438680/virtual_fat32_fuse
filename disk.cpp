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
#include "u16str.h"

std::mutex global_mtx;

fat32::path parse_path(std::string_view path)
{
    fat32::path res;
    std::string buf;
    for (auto c : path)
    {
        if (c == '/')
        {
            if (!buf.empty())
            {
                res.emplace_back(local2wide(buf));
                buf.clear();
            }
        }
        else
        {
            buf.push_back(c);
        }
    }
    if (!buf.empty())
        res.emplace_back(local2wide(buf));
    return res;
}

const char *set_dev_name(const char *name = nullptr)
{
    static char dev_name[4096] = "disk.img";
    if (name)
    {
        strncpy(dev_name, name, 4095);
    }
    return dev_name;
}

dev_io::dev_t &get_dev()
{
    static dev_io::dev_t dev(set_dev_name());
    if (!dev)
    {
        log_msg("error: open device failed! %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return dev;
}

static int vfat_fgetattr(const char *path, struct stat *stbuf, fuse_file_info *fi)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return get_dev().fstat(fi->fh, stbuf);
}

static int vfat_getattr(const char *path, struct stat *stbuf)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return get_dev().stat(parse_path(path), stbuf);
}

static int vfat_access(const char *path, int mask)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    if (mask == F_OK)
    {
        return get_dev().access(parse_path(path));
    }
    return 0;
}

static int vfat_opendir(const char *path, struct fuse_file_info *fi)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return get_dev().open(parse_path(path), &fi->fh);
}

static int vfat_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t, struct fuse_file_info *fi)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    fat32::dir_info entries;
    auto ret = get_dev().readdir(fi->fh, entries);
    for (const auto &entry : entries)
    {
        if (filler(buf, wide2local(entry.name).c_str(), &entry.info, 0))
            break;
    }
    return ret;
}

static int vfat_releasedir(const char *path, struct fuse_file_info *fi)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    get_dev().close(fi->fh);
    return 0;
}

static int vfat_mknod(const char *path, mode_t mode, dev_t rdev)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return get_dev().mknod(parse_path(path));
}

static int vfat_mkdir(const char *path, mode_t mode)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return get_dev().mkdir(parse_path(path));
}

static int vfat_unlink(const char *path)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return get_dev().unlink(parse_path(path));
}

static int vfat_rmdir(const char *path)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return get_dev().rmdir(parse_path(path));
}

static int vfat_rename(const char *from, const char *to)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("from: %s\n", from);
    log_msg("to: %s\n", to);
    return get_dev().rename(parse_path(from), parse_path(to));
}

static int vfat_chmod(const char *path, mode_t mode)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return 0;
}

static int vfat_chown(const char *path, uid_t uid, gid_t gid)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return 0;
}

static int vfat_ftruncate(const char *path, off_t size, fuse_file_info *fi)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return get_dev().ftruncate(fi->fh, size);
}

static int vfat_truncate(const char *path, off_t size)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return get_dev().truncate(parse_path(path), size);
}

static int vfat_open(const char *path, struct fuse_file_info *fi)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return get_dev().open(parse_path(path), &fi->fh);
}

static int vfat_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return get_dev().read(fi->fh, offset, size, buf);
}

static int vfat_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return get_dev().write(fi->fh, offset, size, buf);
}

static int vfat_statfs(const char *path, struct statvfs *stbuf)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return get_dev().statfs(stbuf);
}

static int vfat_utimens(const char *path, const struct timespec tv[2])
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    return get_dev().utimens(parse_path(path), tv);
}

static int vfat_release(const char *path, struct fuse_file_info *fi)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    get_dev().close(fi->fh);
    return 0;
}

static int vfat_fsync(const char *path, int, struct fuse_file_info *)
{
    std::lock_guard<std::mutex> g(global_mtx);
    log_info();
    log_msg("    path: %s\n", path);
    get_dev().flush();
    return 0;
}

static struct fuse_operations operation;

int main(int argc, char *argv[])
{
    memset(&operation, 0, sizeof(operation));
    operation.fgetattr = vfat_fgetattr;
    operation.getattr = vfat_getattr;
    operation.access = vfat_access;
    operation.opendir = vfat_opendir;
    operation.readdir = vfat_readdir;
    operation.releasedir = vfat_releasedir;
    operation.mknod = vfat_mknod;
    operation.mkdir = vfat_mkdir;
    operation.unlink = vfat_unlink;
    operation.rmdir = vfat_rmdir;
    operation.rename = vfat_rename;
    operation.chmod = vfat_chmod;
    operation.chown = vfat_chown;
    operation.ftruncate = vfat_ftruncate;
    operation.truncate = vfat_truncate;
    operation.open = vfat_open;
    operation.read = vfat_read;
    operation.write = vfat_write;
    operation.statfs = vfat_statfs;
    operation.utimens = vfat_utimens;
    operation.release = vfat_release;
    operation.fsync = vfat_fsync;
    get_dev();
    open_log_file();
    return fuse_main(argc, argv, &operation, NULL);
}
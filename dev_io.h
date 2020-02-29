#ifndef DEV_IO_H
#define DEV_IO_H
#include <mutex>
#include <map>
#include <stdexcept>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include "fat32.h"

typedef unsigned long DWORD;
typedef unsigned char BYTE;

namespace dev_io
{

class disk_error : public std::runtime_error
{
public:
    enum error_t
    {
        DISK_NOT_FOUND,
        DISK_OPEN_ERROR,
        DISK_SEEK_ERROR,
        DISK_EXTEND_ERROR,
        DISK_GET_SIZE_ERROR,
        DISK_READ_ERROR,
        DISK_WRITE_ERROR,
        DISK_SIGNATURE_ERROR,
    };
    disk_error(error_t err) noexcept : std::runtime_error(err_msg(err)), err(err) {}
    error_t get_error_type() const noexcept { return err; }

private:
    static const char *err_msg(error_t err)
    {
        static const char *msg_table[] =
            {
                "disk not found",
                "disk open error",
                "disk seek error",
                "disk extend error",
                "disk get size error",
                "disk read error",
                "disk write error",
                "disk signature error",
            };
        return msg_table[static_cast<int>(err)];
    }

    error_t err;
};

class dev_t
{
public:
    dev_t(const char *dev_name, uint32_t tot_block, uint16_t block_size);
    dev_t(const char *dev_name);
    dev_t(dev_t &&dev) = delete;
    dev_t(const dev_t &dev) = delete;
    dev_t &operator=(dev_t &&dev) = delete;
    dev_t &operator=(const dev_t &dev) = delete;
    ~dev_t();

    operator bool() const noexcept;

    int stat(const fat32::path &path, struct stat *stbuf);
    int fstat(uint64_t fd, struct stat *stbuf);
    int access(const fat32::path &path);
    int opendir(const fat32::path &path, uint64_t *fh);
    int readdir(uint64_t fd, fat32::dir_info &entries);
    void closedir(uint64_t fd);
    int open(const fat32::path &path, int flag, uint64_t *fh);
    void close(uint64_t fd);
    int mknod(const fat32::path &path, mode_t mode);
    int mkdir(const fat32::path &path, mode_t mode);
    int unlink(const fat32::path &path);
    int rmdir(const fat32::path &path);
    int rename(const fat32::path &from, const fat32::path &to);
    int truncate(const fat32::path &path, off_t offset);
    int read(uint64_t fd, int64_t offset, uint32_t len, void *buf);
    int write(uint64_t fd, int64_t offset, uint32_t len, const void *buf);
    int statfs(struct statvfs *stbuf);
    int utimens(const fat32::path &path, const struct timespec tv[2]);
    void flush();
    void clear();

private:
    static int32_t dev_read(int fd, uint64_t offset, uint32_t size, void *buf);
    static int32_t dev_write(int fd, uint64_t offset, uint32_t size, const void *buf);
    int32_t read_block(uint32_t block_no, void *buf) const;
    int32_t write_block(uint32_t block_no, const void *buf) const;
    int32_t read_clus(uint32_t clus_no, void *buf) const;
    int32_t write_clus(uint32_t clus_no, const void *buf) const;
    uint32_t get_root_clus() const noexcept;
    uint32_t get_vol_id() const noexcept;
    uint32_t get_fat(uint32_t fat_no) const;
    void set_fat(uint32_t fat_no, uint32_t value);

    void format(uint32_t tot_block, uint16_t block_size);
    void clac_info();

    std::unique_ptr<fat32::file_node> open_file(fat32::file_node *parent, fat32::Entry_Info *pinfo);
    void save(fat32::file_node *node);
    void clear_node(fat32::file_node *node);
    uint32_t next_free();
    void free_clus(uint32_t clus_no);
    void extend(fat32::file_node *node, uint32_t clus_count);
    void shrink(fat32::file_node *node, uint32_t clus_count);
    void add_entry(fat32::file_node *node, fat32::Entry_Info *pinfo, int have_long, bool replace);
    void remove_entry(fat32::file_node *node, std::wstring_view name);
    void gen_short(std::wstring_view name, fat32::file_node *node, char *short_name);
    int32_t DirEntry2EntryInfo(const fat32::DIR_Entry *pdir, fat32::Entry_Info *pinfo);

    int dev_img;
    fat32::BPB_t BPB;
    fat32::FSInfo_t FSInfo;
    std::vector<uint32_t> FAT_Table;
    std::set<uint64_t> open_file_table;
    std::set<std::unique_ptr<fat32::file_node>> delete_file_table;
    std::unique_ptr<fat32::file_node> root;
    uint32_t tot_block;
    uint16_t block_size;
    uint8_t sec_per_clus;
    uint32_t clus_size;
    uint32_t data_begin;
    uint32_t count_of_cluster;
    fat32::Entry_Info root_info;
    bool cleared;
};

} // namespace dev_io

#endif
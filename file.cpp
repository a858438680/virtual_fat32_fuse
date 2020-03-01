#include <numeric>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "dev_io.h"
#include "log.h"
#include "u16str.h"

static const char this_folder[] = {0x2e, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
static const char parent_folder[] = {0x2e, 0x2e, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};

namespace dev_io
{

unsigned char ChkSum(unsigned char *pFcbName)
{
    short FcbNameLen;
    unsigned char Sum;
    Sum = 0;
    for (FcbNameLen = 11; FcbNameLen != 0; FcbNameLen--)
    {
        // NOTE: The operation is an unsigned char rotate right
        Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
    }
    return (Sum);
}

void EntryInfo2DirEntry(fat32::Entry_Info *pinfo, fat32::DIR_Entry *pentry, int have_long)
{
    if (have_long)
    {
        size_t index = 0;
        auto name_len = u16len(pinfo->name);
        auto len = (name_len + 13 - 1) / 13;
        auto padding_len = (len * 13 < 256 ? len * 13 : 256) - name_len;
        if (padding_len > 1)
        {
            memset(pinfo->name + name_len + 1, 0xff, (padding_len - 1) * sizeof(char16_t));
        }
        auto ldir = (fat32::LDIR_Entry *)pentry;
        pentry += len;
        auto chksum = ChkSum((unsigned char *)pinfo->short_name);
        memset(ldir, 0xff, len * sizeof(fat32::LDIR_Entry));
        for (size_t i = 0; i < len; ++i)
        {
            ldir[len - i - 1].LDIR_Ord = i + 1;
            memcpy(ldir[len - i - 1].LDIR_Name1, pinfo->name + index, sizeof(ldir[len - i - 1].LDIR_Name1));
            ldir[len - i - 1].LDIR_Attr = 0xf;
            ldir[len - i - 1].LDIR_Type = 0;
            ldir[len - i - 1].LDIR_Chksum = chksum;
            memcpy(ldir[len - i - 1].LDIR_Name2, pinfo->name + index + 5, sizeof(ldir[len - i - 1].LDIR_Name2));
            ldir[len - i - 1].LDIR_FstClusLO = 0;
            memcpy(ldir[len - i - 1].LDIR_Name3, pinfo->name + index + 11, sizeof(ldir[len - i - 1].LDIR_Name3));
            index += 13;
        }
        ldir[0].LDIR_Ord |= 0x40;
    }
    memcpy(pentry->DIR_Name, pinfo->short_name, sizeof(pinfo->short_name));
    pentry->DIR_Attr = S_ISREG(pinfo->info.st_mode) ? 0x00 : 0x10;
    pentry->DIR_FstClusHI = ((pinfo->first_clus >> 16) & 0xffff);
    pentry->DIR_FstClusLO = (pinfo->first_clus & 0xffff);
    pentry->DIR_FileSize = pinfo->info.st_size;
    auto broken_time = localtime(&pinfo->info.st_ctim.tv_sec);
    pentry->DIR_CrtDate = ((broken_time->tm_year - 80) << 9) + ((broken_time->tm_mon + 1) << 5) + broken_time->tm_mday + 1;
    pentry->DIR_CrtTime = (broken_time->tm_hour << 11) + (broken_time->tm_min << 5) + (broken_time->tm_sec / 2);
    pentry->DIR_CrtTimeTenth = (broken_time->tm_sec % 2) * 100 + pinfo->info.st_ctim.tv_nsec / 10000000;
    broken_time = localtime(&pinfo->info.st_mtim.tv_sec);
    pentry->DIR_WrtDate = ((broken_time->tm_year - 80) << 9) + ((broken_time->tm_mon + 1) << 5) + broken_time->tm_mday + 1;
    pentry->DIR_WrtTime = (broken_time->tm_hour << 11) + (broken_time->tm_min << 5) + (broken_time->tm_sec / 2);
    broken_time = localtime(&pinfo->info.st_atim.tv_sec);
    pentry->DIR_LstAccDate = ((broken_time->tm_year - 80) << 9) + ((broken_time->tm_mon + 1) << 5) + broken_time->tm_mday + 1;
}

void dev_t::gen_short(std::u16string_view name, fat32::file_node *node, char *short_name)
{
    char name_tmp[11];
    memset(name_tmp, 0x20, sizeof(name_tmp));
    char *main_part = name_tmp;
    char *ext_part = name_tmp + 8;
    auto last_dot = name.find_last_of(u'.');
    if (last_dot != std::u16string_view::npos && last_dot != name.length() - 1)
    {
        size_t index = 0;
        for (auto i = last_dot + 1; i < name.length(); ++i)
        {
            if (name[i] >= 256)
            {
                if (index + 2 > 3)
                {
                    break;
                }
                else
                {
                    *(char16_t *)(ext_part + index) = name[i];
                    index += 2;
                }
            }
            else
            {
                if (index + 1 > 3)
                {
                    break;
                }
                else
                {
                    ext_part[index] = toupper(name[i]);
                    index += 1;
                }
            }
        }
    }
    else
    {
        last_dot = name.length();
    }
    size_t len = 0;
    for (size_t i = 0; i < last_dot; ++i)
    {
        len += (size_t)(name[i] < 256 ? 1 : 2);
    }
    int dup = len > 8;
    bool ok;
    do
    {
        ok = true;
        if (dup)
        {
            memset(main_part, 0x20, 8);
            size_t num_len = 2;
            auto copy = dup;
            while (copy /= 10)
            {
                ++num_len;
            }
            auto max = 8 - num_len;
            size_t index = 0;
            for (auto i = 0; i < last_dot; ++i)
            {
                if (name[i] >= 256)
                {
                    if (index + 2 > max)
                    {
                        break;
                    }
                    else
                    {
                        *(char16_t *)(main_part + index) = name[i];
                        index += 2;
                    }
                }
                else
                {
                    if (index + 1 > max)
                    {
                        break;
                    }
                    else
                    {
                        main_part[index] = toupper(name[i]);
                        index += 1;
                    }
                }
            }
            max = index + num_len;
            copy = dup;
            while (copy)
            {
                main_part[--max] = '0' + copy % 10;
                copy /= 10;
            }
            main_part[--max] = '~';
        }
        else
        {
            size_t index = 0;
            for (auto i = 0; i < last_dot; ++i)
            {
                if (name[i] >= 256)
                {
                    *(char16_t *)(main_part + index) = name[i];
                    index += 2;
                }
                else
                {
                    main_part[index] = toupper(name[i]);
                    index += 1;
                }
            }
        }
        fat32::Entry_Info info;
        size_t entry_index = 0;
        fat32::dir_info dir_entries;
        readdir((uint64_t)node, dir_entries);
        for (const auto &e : dir_entries)
        {
            if (memcmp(name_tmp, e.short_name, sizeof(name_tmp)) == 0 && name != e.name)
            {
                ++dup;
                ok = false;
                break;
            }
        }
    } while (!ok);
    memcpy(short_name, name_tmp, sizeof(name_tmp));
}

int dev_t::fstat(uint64_t fd, struct stat *stbuf)
{
    cleared = false;
    if (open_file_table.find(fd) != open_file_table.end())
    {
        auto p = (fat32::file_node *)fd;
        memcpy(stbuf, &p->info.info, sizeof(struct stat));
        return 0;
    }
    else
    {
        throw fat32::file_error(fat32::file_error::INVALID_FILE_DISCRIPTOR);
    }
}

int dev_t::stat(const fat32::path &path, struct stat *stbuf)
{
    uint64_t fd;
    int ret = open(path, &fd);
    if (ret < 0)
        return ret;
    ret = fstat(fd, stbuf);
    close(fd);
    return ret;
}

int dev_t::access(const fat32::path &path)
{
    uint64_t fd;
    int ret = open(path, &fd);
    if (ret < 0)
        return -1;
    close(fd);
    return 0;
}

int dev_t::readdir(uint64_t fd, fat32::dir_info &entries)
{
    if (open_file_table.find(fd) != open_file_table.end())
    {
        auto p = (fat32::file_node *)fd;
        if (!S_ISDIR(p->info.info.st_mode))
        {
            return -EBADF;
        }
        if (p->parent)
        {
            fat32::Entry_Info tmp_info;
            memcpy(&tmp_info, &p->info, sizeof(fat32::Entry_Info));
            u16cpy(tmp_info.name, u".");
            memcpy(tmp_info.short_name, this_folder, sizeof(this_folder));
            add_entry(p, &tmp_info, 0, true);
            memcpy(&tmp_info, &p->parent->info, sizeof(fat32::Entry_Info));
            u16cpy(tmp_info.name, u"..");
            memcpy(tmp_info.short_name, parent_folder, sizeof(parent_folder));
            add_entry(p, &tmp_info, 0, true);
        }
        for (const auto &c : p->children)
        {
            add_entry(p, &c.second->info, 1, true);
        }
        fat32::dir_info res;
        fat32::Entry_Info buf;
        size_t index = 0;
        int32_t ret;
        while (index < p->entries.size() && (ret = DirEntry2EntryInfo(p->entries.data() + index, &buf)))
        {
            if (ret < 0)
            {
                index -= ret;
            }
            else
            {
                res.push_back(buf);
                index += ret;
            }
        }
        entries = std::move(res);
        return 0;
    }
    else
    {
        throw fat32::file_error(fat32::file_error::INVALID_FILE_DISCRIPTOR);
    }
}

int dev_t::open(const fat32::path &path, uint64_t *fh)
{
    if (path.empty())
    {
        root->ref_count.fetch_add(1, std::memory_order_relaxed);
        open_file_table.insert((uint64_t)root.get());
        if (fh)
            *fh = (uint64_t)root.get();
        return 0;
    }
    else
    {
        auto last = root.get();
        decltype(last->children.begin()) itr;
        bool end = false;
        for (const auto &name : path)
        {
            if (!end)
            {
                end = (itr = last->children.find(name)) == last->children.end();
            }
            if (!end)
            {
                last = itr->second.get();
            }
            else
            {
                if (S_ISDIR(last->info.info.st_mode))
                {
                    fat32::Entry_Info buf;
                    size_t index = 0;
                    auto find = false;
                    int32_t ret;
                    while (index < last->entries.size() && (ret = DirEntry2EntryInfo(last->entries.data() + index, &buf)))
                    {
                        if (ret < 0)
                        {
                            index -= ret;
                        }
                        else
                        {
                            if (name == buf.name)
                            {
                                auto ptr = open_file(last, &buf);
                                auto temp = ptr.get();
                                last->children.insert(std::make_pair(name, std::move(ptr)));
                                open_file_table.insert((uint64_t)temp);
                                last = temp;
                                find = true;
                                break;
                            }
                            index += ret;
                        }
                    }
                    if (!find)
                    {
                        clear_node(last);
                        return -ENOENT;
                    }
                }
                else
                {
                    clear_node(last);
                    return -ENOTDIR;
                }
            }
        }
        last->ref_count.fetch_add(1, std::memory_order_relaxed);
        open_file_table.insert((uint64_t)last);
        if (fh)
            *fh = (uint64_t)last;
        return 0;
    }
}

void dev_t::close(uint64_t fd)
{
    if (open_file_table.find(fd) != open_file_table.end())
    {
        auto p = (fat32::file_node *)fd;
        p->ref_count.fetch_sub(1, std::memory_order_acquire);
        clear_node(p);
    }
    else
    {
        throw fat32::file_error(fat32::file_error::INVALID_FILE_DISCRIPTOR);
    }
}

int dev_t::mknod(const fat32::path &path)
{
    cleared = false;
    if (path.empty())
    {
        return -EEXIST;
    }
    else
    {
        auto copy = path;
        copy.pop_back();
        uint64_t fd;
        int ret = open(copy, &fd);
        if (ret < 0)
            return ret;
        auto node = (fat32::file_node *)fd;
        if (!S_ISDIR(node->info.info.st_mode))
        {
            close(fd);
            return -ENOTDIR;
        }
        fat32::dir_info entries;
        ret = readdir(fd, entries);
        if (ret < 0)
        {
            close(fd);
            return -ENOTDIR;
        }
        for (const auto &e : entries)
        {
            if (e.name == path.back())
            {
                close(fd);
                return -EEXIST;
            }
        }
        fat32::Entry_Info info;
        u16cpy(info.name, path.back().c_str());
        gen_short(path.back(), node, info.short_name);
        info.first_clus = 0;
        info.info.st_dev = get_vol_id();
        info.info.st_ino = 0;
        info.info.st_mode = 0777 | S_IFREG;
        info.info.st_nlink = 1;
        info.info.st_uid = 0;
        info.info.st_gid = 0;
        info.info.st_rdev = 0;
        info.info.st_size = 0;
        info.info.st_blksize = block_size;
        info.info.st_blocks = 0;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        auto broken_time = localtime(&tv.tv_sec);
        info.info.st_ctim.tv_sec = tv.tv_sec;
        info.info.st_ctim.tv_nsec = tv.tv_usec / 10000000 * 10000000;
        broken_time->tm_sec = broken_time->tm_sec / 2 * 2;
        info.info.st_mtim.tv_sec = mktime(broken_time);
        info.info.st_mtim.tv_nsec = 0;
        broken_time->tm_hour = 0;
        broken_time->tm_min = 0;
        broken_time->tm_sec = 0;
        info.info.st_atim.tv_sec = mktime(broken_time);
        info.info.st_atim.tv_nsec = 0;
        add_entry(node, &info, 1, false);
        return 0;
    }
}

int dev_t::mkdir(const fat32::path &path)
{
    cleared = false;
    if (path.empty())
    {
        return -EEXIST;
    }
    else
    {
        auto copy = path;
        copy.pop_back();
        uint64_t fd;
        int ret = open(copy, &fd);
        if (ret < 0)
            return ret;
        auto node = (fat32::file_node *)fd;
        if (!S_ISDIR(node->info.info.st_mode))
        {
            close(fd);
            return -ENOTDIR;
        }
        fat32::dir_info entries;
        ret = readdir(fd, entries);
        if (ret < 0)
        {
            close(fd);
            return -ENOTDIR;
        }
        for (const auto &e : entries)
        {
            if (e.name == path.back())
            {
                close(fd);
                return -EEXIST;
            }
        }
        fat32::Entry_Info info;
        u16cpy(info.name, path.back().c_str());
        gen_short(path.back(), node, info.short_name);
        info.first_clus = 0;
        info.info.st_dev = get_vol_id();
        info.info.st_ino = 0;
        info.info.st_mode = 0555 | S_IFDIR;
        info.info.st_nlink = 2;
        info.info.st_uid = 0;
        info.info.st_gid = 0;
        info.info.st_rdev = 0;
        info.info.st_size = 0;
        info.info.st_blksize = block_size;
        info.info.st_blocks = 0;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        auto broken_time = localtime(&tv.tv_sec);
        info.info.st_ctim.tv_sec = tv.tv_sec;
        info.info.st_ctim.tv_nsec = tv.tv_usec / 10000000 * 10000000;
        broken_time->tm_sec = broken_time->tm_sec / 2 * 2;
        info.info.st_mtim.tv_sec = mktime(broken_time);
        info.info.st_mtim.tv_nsec = 0;
        broken_time->tm_hour = 0;
        broken_time->tm_min = 0;
        broken_time->tm_sec = 0;
        info.info.st_atim.tv_sec = mktime(broken_time);
        info.info.st_atim.tv_nsec = 0;
        add_entry(node, &info, 1, false);
        ret = open(path, &fd);
        if (ret < 0)
        {
            close((uint64_t)node);
            throw fat32::file_error(fat32::file_error::FILE_NOT_FOUND);
        }
        auto temp = (fat32::file_node *)fd;
        fat32::Entry_Info tmp_info;
        memcpy(&tmp_info, &temp->info, sizeof(fat32::Entry_Info));
        u16cpy(tmp_info.name, u".");
        memcpy(tmp_info.short_name, this_folder, sizeof(this_folder));
        add_entry(temp, &tmp_info, 0, true);
        memcpy(&tmp_info, &node->info, sizeof(fat32::Entry_Info));
        u16cpy(tmp_info.name, u"..");
        memcpy(tmp_info.short_name, parent_folder, sizeof(parent_folder));
        add_entry(temp, &tmp_info, 0, true);
        close(fd);
        close((uint64_t)node);
        return 0;
    }
}

int dev_t::unlink(const fat32::path &path)
{
    cleared = false;
    if (path.empty())
    {
        return -EACCES;
    }
    else
    {
        uint64_t fd;
        int ret = open(path, &fd);
        if (ret < 0)
            return ret;
        auto node = (fat32::file_node *)fd;
        if (S_ISDIR(node->info.info.st_mode))
        {
            close(fd);
            return -EISDIR;
        }
        auto parent = node->parent;
        node->parent = nullptr;
        auto itr = parent->children.find(node->info.name);
        auto handle = (uint64_t)itr->second.get();
        delete_file_table.insert(std::make_pair(handle, std::move(itr->second)));
        parent->children.erase(itr);
        remove_entry(parent, node->info.name);
        close(fd);
        return 0;
    }
}

int dev_t::rmdir(const fat32::path &path)
{
    cleared = false;
    if (path.empty())
    {
        return -EACCES;
    }
    else
    {
        uint64_t fd;
        int ret = open(path, &fd);
        if (ret < 0)
            return ret;
        auto node = (fat32::file_node *)fd;
        if (!S_ISDIR(node->info.info.st_mode))
        {
            close(fd);
            return -ENOTDIR;
        }
        fat32::dir_info entries;
        ret = readdir(fd, entries);
        if (ret < 0)
        {
            close(fd);
            return -ENOTDIR;
        }
        if (entries.size() > 2)
        {
            close(fd);
            return -ENOTEMPTY;
        }
        auto parent = node->parent;
        node->parent = nullptr;
        auto itr = parent->children.find(node->info.name);
        auto handle = (uint64_t)itr->second.get();
        delete_file_table.insert(std::make_pair(handle, std::move(itr->second)));
        parent->children.erase(itr);
        remove_entry(parent, node->info.name);
        close(fd);
        return 0;
    }
}

int dev_t::rename(const fat32::path &from, const fat32::path &to)
{
    cleared = false;
    if (from.empty() || to.empty())
    {
        return -EACCES;
    }
    else
    {
        uint64_t fd, to_fd;
        int ret = open(from, &fd);
        if (ret < 0)
            return ret;
        auto node = (fat32::file_node *)fd;
        auto copy = to;
        copy.pop_back();
        ret = open(copy, &to_fd);
        if (ret < 0)
        {
            close(fd);
            return ret;
        }
        auto to_node = (fat32::file_node *)to_fd;
        if (!S_ISDIR(to_node->info.info.st_mode))
        {
            close(to_fd);
            close(fd);
            return -ENOTDIR;
        }
        fat32::dir_info entries;
        ret = readdir(to_fd, entries);
        if (ret < 0)
        {
            close(to_fd);
            close(fd);
            return -ENOTDIR;
        }
        for (const auto &e : entries)
        {
            if (e.name == to.back())
            {
                uint64_t new_fd;
                ret = open(to, &new_fd);
                if (ret < 0)
                {
                    close(to_fd);
                    close(fd);
                    throw fat32::file_error(fat32::file_error::FILE_NOT_FOUND);
                }
                auto new_node = (fat32::file_node *)new_fd;
                if (S_ISDIR(node->info.info.st_mode))
                {
                    if (!S_ISDIR(new_node->info.info.st_mode))
                    {
                        close(new_fd);
                        close(to_fd);
                        close(fd);
                        return -EEXIST;
                    }
                    fat32::dir_info entries;
                    ret = readdir(new_fd, entries);
                    if (entries.size() > 2)
                    {
                        close(new_fd);
                        close(to_fd);
                        close(fd);
                        return -EEXIST;
                    }
                }
                new_node->parent = nullptr;
                auto itr = to_node->children.find(new_node->info.name);
                auto handle = (uint64_t)itr->second.get();
                delete_file_table.insert(std::make_pair(handle, std::move(itr->second)));
                to_node->children.erase(itr);
                close(new_fd);
                break;
            }
        }
        auto parent = node->parent;
        node->parent = to_node;
        auto itr = parent->children.find(node->info.name);
        add_entry(to_node, &node->info, 1, true);
        to_node->children.insert(std::make_pair(node->info.name, std::move(itr->second)));
        parent->children.erase(itr);
        remove_entry(parent, node->info.name);
        close(to_fd);
        close(fd);
        return 0;
    }
}

int dev_t::ftruncate(uint64_t fd, off_t offset)
{
    cleared = false;
    if (open_file_table.find(fd) != open_file_table.end())
    {
        auto p = (fat32::file_node *)fd;
        auto clus_end = (offset + clus_size - 1) / clus_size;
        if (clus_end > p->alloc.size())
        {
            auto origin = p->alloc.size();
            extend(p, clus_end);
            std::vector<char> buf(clus_size, 0);
            for (uint32_t i = origin; i < clus_end; ++i)
            {
                write_clus(p->alloc[i], buf.data());
            }
            p->info.info.st_size = offset;
        }
        if (clus_end < p->alloc.size())
        {
            shrink(p, clus_end);
            p->info.info.st_size = offset;
        }
        return 0;
    }
    else
    {
        throw fat32::file_error(fat32::file_error::INVALID_FILE_DISCRIPTOR);
    }
}

int dev_t::truncate(const fat32::path &path, off_t offset)
{
    cleared = false;
    uint64_t fd;
    int ret = open(path, &fd);
    if (ret < 0)
        return ret;
    ftruncate(fd, offset);
    close(fd);
    return 0;
}

int dev_t::read(uint64_t fd, int64_t offset, uint32_t len, void *buffer)
{
    if (open_file_table.find(fd) != open_file_table.end())
    {
        auto p = (fat32::file_node *)fd;
        uint64_t file_size = p->info.info.st_size;
        int64_t left_border = offset > 0 ? offset : 0;
        int64_t right_border = (offset + len) < file_size ? (offset + len) : file_size;
        if (left_border >= right_border)
        {
            return 0;
        }
        std::vector<char> buf(clus_size);
        uint32_t begin_clus = left_border / clus_size;
        uint32_t end_clus = (right_border + clus_size - 1) / clus_size;
        uint32_t index = 0;
        for (auto i = begin_clus; i < end_clus; ++i)
        {
            read_clus(p->alloc[i], buf.data());
            uint32_t begin = 0, end = clus_size;
            if (i == begin_clus)
            {
                begin = left_border % clus_size;
            }
            if (i == end_clus - 1)
            {
                end = (right_border - 1) % clus_size + 1;
            }
            auto size = end - begin;
            memcpy((char *)buffer + index, buf.data() + begin, size);
            index += size;
        }
        time_t t = time(NULL);
        auto broken_time = localtime(&t);
        broken_time->tm_hour = 0;
        broken_time->tm_min = 0;
        broken_time->tm_sec = 0;
        p->info.info.st_atim.tv_sec = mktime(broken_time);
        p->info.info.st_atim.tv_nsec = 0;
        return index;
    }
    else
    {
        throw fat32::file_error(fat32::file_error::INVALID_FILE_DISCRIPTOR);
    }
}

int dev_t::write(uint64_t fd, int64_t offset, uint32_t len, const void *buffer)
{
    cleared = false;
    if (open_file_table.find(fd) != open_file_table.end())
    {
        auto p = (fat32::file_node *)fd;
        if (!len)
        {
            return 0;
        }
        uint64_t file_size = p->info.info.st_size;
        uint64_t left_border = (offset > 0 ? offset : 0);
        uint64_t right_border = left_border + len;
        std::vector<char> buf(clus_size, 0);
        uint32_t begin_clus = left_border / clus_size;
        uint32_t end_clus = (right_border + clus_size - 1) / clus_size;
        uint32_t index = 0;
        if (end_clus > p->alloc.size())
        {
            auto origin = p->alloc.size();
            extend(p, end_clus);
            if (begin_clus >= origin)
            {
                for (auto i = origin; i < begin_clus; ++i)
                {
                    write_clus(p->alloc[i], buf.data());
                }
            }
        }
        for (auto i = begin_clus; i < end_clus; ++i)
        {
            uint32_t begin = 0, end = clus_size;
            if (i == begin_clus)
            {
                begin = left_border % clus_size;
            }
            if (i == end_clus - 1)
            {
                end = (right_border - 1) % clus_size + 1;
            }
            auto size = end - begin;
            if (size < clus_size)
            {
                read_clus(p->alloc[i], buf.data());
                memcpy(buf.data() + begin, (char *)buffer + index, size);
                write_clus(p->alloc[i], buf.data());
            }
            else
            {
                write_clus(p->alloc[i], (char *)buffer + index);
            }
            index += size;
        }
        if (right_border > file_size)
        {
            p->info.info.st_size = right_border;
        }
        time_t t = time(NULL);
        auto broken_time = localtime(&t);
        broken_time->tm_sec = broken_time->tm_sec / 2 * 2;
        p->info.info.st_mtim.tv_sec = mktime(broken_time);
        p->info.info.st_mtim.tv_nsec = 0;
        broken_time->tm_hour = 0;
        broken_time->tm_min = 0;
        broken_time->tm_sec = 0;
        p->info.info.st_atim.tv_sec = mktime(broken_time);
        p->info.info.st_atim.tv_nsec = 0;
        return index;
    }
    else
    {
        throw fat32::file_error(fat32::file_error::INVALID_FILE_DISCRIPTOR);
    }
}

int dev_t::statfs(struct statvfs *stbuf)
{
    stbuf->f_bavail = FSInfo.FSI_FreeCount;
    stbuf->f_bfree = FSInfo.FSI_FreeCount;
    stbuf->f_blocks = count_of_cluster;
    stbuf->f_bsize = clus_size;
    stbuf->f_favail = FSInfo.FSI_FreeCount;
    stbuf->f_ffree = FSInfo.FSI_FreeCount;
    stbuf->f_files = count_of_cluster;
    stbuf->f_flag = 0;
    stbuf->f_frsize = block_size;
    stbuf->f_fsid = 0;
    return 0;
}

int dev_t::utimens(const fat32::path &path, const struct timespec tv[2])
{
    cleared = false;
    uint64_t fd;
    int ret = open(path, &fd);
    if (ret < 0)
        return ret;
    auto p = (fat32::file_node *)fd;
    auto broken_time = localtime(&tv[0].tv_sec);
    broken_time->tm_sec = 0;
    broken_time->tm_min = 0;
    broken_time->tm_hour = 0;
    p->info.info.st_atim.tv_sec = mktime(broken_time);
    p->info.info.st_atim.tv_nsec = 0;
    broken_time = localtime(&tv[1].tv_sec);
    broken_time->tm_sec = broken_time->tm_sec / 2 * 2;
    p->info.info.st_mtim.tv_sec = mktime(broken_time);
    p->info.info.st_mtim.tv_nsec = 0;
    close(fd);
    return 0;
}

void dev_t::flush() {}

void dev_t::clear()
{
    if (!cleared)
    {
        write_block(0, &BPB);
        fat32::BPB_t BPB_Backup;
        memcpy(&BPB_Backup, &BPB, sizeof(BPB));
        BPB_Backup.BPB_BkBootSec = 0;
        write_block(0, &BPB_Backup);
        write_block(BPB.BPB_FSInfo, &FSInfo);
        for (uint32_t i = 0; i < BPB.BPB_FATSz32; ++i)
        {
            write_block(i + BPB.BPB_RsvdSecCnt, FAT_Table.data() + i * block_size / sizeof(uint32_t));
            write_block(i + BPB.BPB_RsvdSecCnt + BPB.BPB_FATSz32, FAT_Table.data() + i * block_size / sizeof(uint32_t));
        }
        save(root.get());
        flush();
        cleared = true;
    }
}

std::unique_ptr<fat32::file_node> dev_t::open_file(fat32::file_node *parent, fat32::Entry_Info *pinfo)
{
    auto res = std::make_unique<fat32::file_node>(parent);
    auto first_clus = pinfo->first_clus;
    while (first_clus >= 2 && first_clus < count_of_cluster + 2)
    {
        res->alloc.push_back(first_clus);
        first_clus = get_fat(first_clus);
    }
    memcpy(&res->info, parent ? pinfo : &root_info, sizeof(fat32::Entry_Info));
    if (S_ISDIR(res->info.info.st_mode))
    {
        res->entries.resize(res->alloc.size() * clus_size / sizeof(fat32::DIR_Entry));
        size_t count = 0;
        for (auto clus_no : res->alloc)
        {
            read_clus(clus_no, res->entries.data() + count);
            count += clus_size / sizeof(fat32::DIR_Entry);
        }
    }
    return std::move(res);
}

void dev_t::save(fat32::file_node *node)
{
    if (node)
    {
        if (S_ISDIR(node->info.info.st_mode))
        {
            if (node->parent)
            {
                fat32::Entry_Info tmp_info;
                memcpy(&tmp_info, &node->info, sizeof(fat32::Entry_Info));
                u16cpy(tmp_info.name, u".");
                memcpy(tmp_info.short_name, this_folder, sizeof(this_folder));
                add_entry(node, &tmp_info, 0, true);
                memcpy(&tmp_info, &node->parent->info, sizeof(fat32::Entry_Info));
                u16cpy(tmp_info.name, u"..");
                memcpy(tmp_info.short_name, parent_folder, sizeof(parent_folder));
                add_entry(node, &tmp_info, 0, true);
            }
            for (auto &p : node->children)
            {
                save(p.second.get());
            }
            size_t index = 0;
            for (auto clus : node->alloc)
            {
                write_clus(clus, node->entries.data() + index * (clus_size / sizeof(fat32::DIR_Entry)));
                ++index;
            }
        }
        if (node->parent)
        {
            add_entry(node->parent, &node->info, 1, true);
        }
    }
}

void dev_t::clear_node(fat32::file_node *node)
{
    if (!node->ref_count.load(std::memory_order_acquire) && node->children.empty())
    {
        decltype(delete_file_table.begin()) itr;
        if ((itr = delete_file_table.find((uint64_t)node)) != delete_file_table.end())
        {
            if (node->info.first_clus != get_root_clus())
            {
                shrink(node, 0);
                open_file_table.erase((uint64_t)node);
                delete_file_table.erase(itr);
            }
        }
        else
        {
            if (S_ISDIR(node->info.info.st_mode))
            {
                if (node->info.first_clus != get_root_clus())
                {
                    fat32::Entry_Info tmp_info;
                    memcpy(&tmp_info, &node->info, sizeof(fat32::Entry_Info));
                    u16cpy(tmp_info.name, u".");
                    memcpy(tmp_info.short_name, this_folder, sizeof(this_folder));
                    add_entry(node, &tmp_info, 0, true);
                    memcpy(&tmp_info, &node->parent->info, sizeof(fat32::Entry_Info));
                    u16cpy(tmp_info.name, u"..");
                    memcpy(tmp_info.short_name, parent_folder, sizeof(parent_folder));
                    add_entry(node, &tmp_info, 0, true);
                }
                size_t index = 0;
                for (auto clus : node->alloc)
                {
                    write_clus(clus, node->entries.data() + index * (clus_size / sizeof(fat32::DIR_Entry)));
                    ++index;
                }
            }
            if (node->parent)
            {
                std::u16string name = node->info.name;
                auto parent = node->parent;
                add_entry(parent, &node->info, 1, true);
                open_file_table.erase((uint64_t)node);
                parent->children.erase(name);
                clear_node(parent);
            }
        }
    }
}

uint32_t dev_t::next_free()
{
    auto clus = FSInfo.FSI_Nxt_Free;
    while (get_fat(clus))
    {
        ++clus;
    }
    set_fat(clus, 0x0fffffff);
    FSInfo.FSI_Nxt_Free = clus + 1;
    --FSInfo.FSI_FreeCount;
    return clus;
}

void dev_t::free_clus(uint32_t clus_no)
{
    set_fat(clus_no, 0);
    if (clus_no < FSInfo.FSI_Nxt_Free)
    {
        FSInfo.FSI_Nxt_Free = clus_no;
    }
    ++FSInfo.FSI_FreeCount;
}

void dev_t::extend(fat32::file_node *node, uint32_t clus_count)
{
    if (node->alloc.size() < clus_count)
    {
        size_t extend_size = clus_count - node->alloc.size();
        for (size_t i = 0; i < extend_size; ++i)
        {
            auto next = next_free();
            if (node->alloc.empty())
            {
                node->info.first_clus = next;
                node->info.info.st_ino = next;
            }
            else
            {
                set_fat(node->alloc.back(), next);
            }
            node->alloc.push_back(next);
        }
    }
}

void dev_t::shrink(fat32::file_node *node, uint32_t clus_count)
{
    if (node->alloc.size() > clus_count)
    {
        size_t shrink_size = node->alloc.size() - clus_count;
        for (size_t i = 0; i < shrink_size; ++i)
        {
            free_clus(node->alloc.back());
            node->alloc.pop_back();
        }
        if (node->alloc.empty())
        {
            node->info.first_clus = 0;
            node->info.info.st_ino = 0;
        }
        else
        {
            set_fat(node->alloc.back(), 0x0fffffff);
        }
    }
}

void dev_t::add_entry(fat32::file_node *node, fat32::Entry_Info *pinfo, int have_long, bool replace)
{
    size_t entry_len = 1;
    if (have_long)
    {
        entry_len += (u16len(pinfo->name) + 13 - 1) / 13;
    }
    size_t index = 0;
    fat32::Entry_Info buf;
    int32_t ret;
    while (index < node->entries.size() && (ret = DirEntry2EntryInfo(node->entries.data() + index, &buf)))
    {
        if (ret < 0)
        {
            index -= ret;
        }
        else
        {
            if (u16cmp(buf.name, pinfo->name) == 0)
            {
                if (replace)
                {
                    if (ret >= entry_len)
                    {
                        EntryInfo2DirEntry(pinfo, node->entries.data() + index, have_long);
                        for (size_t i = index + entry_len; i < index + ret; ++i)
                        {
                            *(uint8_t *)node->entries[i].DIR_Name = 0xe5;
                        }
                        return;
                    }
                    else
                    {
                        for (size_t i = index; i < index + ret; ++i)
                        {
                            *(uint8_t *)node->entries[i].DIR_Name = 0xe5;
                        }
                    }
                }
                else
                {
                    return;
                }
            }
            index += ret;
        }
    }
    if (node->entries.size() - index < entry_len)
    {
        size_t new_clus_count = ((index + entry_len) * sizeof(fat32::DIR_Entry) + clus_size - 1) / clus_size;
        extend(node, new_clus_count);
        node->entries.resize(new_clus_count * clus_size / sizeof(fat32::DIR_Entry));
        memset(node->entries.data() + index + entry_len, 0, (node->entries.size() - index - entry_len) * sizeof(fat32::DIR_Entry));
    }
    EntryInfo2DirEntry(pinfo, node->entries.data() + index, have_long);
}

void dev_t::remove_entry(fat32::file_node *node, std::u16string_view name)
{
    size_t index = 0;
    fat32::Entry_Info buf;
    int32_t ret;
    while (index < node->entries.size() && (ret = DirEntry2EntryInfo(node->entries.data() + index, &buf)))
    {
        if (ret < 0)
        {
            index -= ret;
        }
        else
        {
            if (buf.name == name)
            {
                if (*(uint8_t *)node->entries[index + ret].DIR_Name == 0)
                {
                    memset(&node->entries[index], 0, ret * sizeof(fat32::DIR_Entry));
                }
                else
                {
                    for (size_t i = index; i < index + ret; ++i)
                    {
                        *(uint8_t *)node->entries[i].DIR_Name = 0xe5;
                    }
                }
                return;
            }
            index += ret;
        }
    }
}

int32_t dev_t::DirEntry2EntryInfo(const fat32::DIR_Entry *pdir, fat32::Entry_Info *pinfo)
{
    if (pdir[0].DIR_Name[0] == 0)
    {
        return 0;
    }
    int count = 0;
    while ((uint8_t)pdir[count].DIR_Name[0] == 0xe5)
    {
        ++count;
    }
    if (count)
    {
        if (pdir[count].DIR_Name[0] == 0)
        {
            return 0;
        }
        return -count;
    }
    char16_t name[256];
    int name_pos = 255;
    int have_long_name = 0;
    fat32::LDIR_Entry *ldir = (fat32::LDIR_Entry *)pdir;
    while ((pdir[count].DIR_Attr & 0x3f) == 0x0f)
    {
        if (ldir[count].LDIR_Ord & 0x40)
        {
            char16_t tmp[13];
            memcpy(tmp + 11, ldir[count].LDIR_Name3, sizeof(ldir[count].LDIR_Name3));
            memcpy(tmp + 5, ldir[count].LDIR_Name2, sizeof(ldir[count].LDIR_Name2));
            memcpy(tmp, ldir[count].LDIR_Name1, sizeof(ldir[count].LDIR_Name1));
            size_t len = u16nlen(tmp, 13);
            name_pos = 255 - len;
            u16ncpy(name + name_pos, tmp, len);
            have_long_name = 1;
        }
        else
        {
            memcpy(name + name_pos - 2, ldir[count].LDIR_Name3, sizeof(ldir[count].LDIR_Name3));
            memcpy(name + name_pos - 8, ldir[count].LDIR_Name2, sizeof(ldir[count].LDIR_Name2));
            memcpy(name + name_pos - 13, ldir[count].LDIR_Name1, sizeof(ldir[count].LDIR_Name1));
            name_pos -= 13;
        }
        ++count;
    }
    memset(pinfo, 0, sizeof(fat32::Entry_Info));
    if (have_long_name)
    {
        u16cpy(pinfo->name, name + name_pos);
    }
    else
    {
        char16_t c;
        int index = 0;
        for (int k = 0; k < 8 && (c = pdir[count].DIR_Name[k]) != 0x20; ++k)
        {
            pinfo->name[index++] = c;
        }
        if (pdir[count].DIR_Name[8] != 0x20)
        {
            pinfo->name[index++] = u'.';
            for (int k = 8; k < 11 && (c = pdir[count].DIR_Name[k]) != 0x20; ++k)
            {
                pinfo->name[index++] = c;
            }
        }
    }
    memcpy(pinfo->short_name, pdir[count].DIR_Name, sizeof(pinfo->short_name));
    pinfo->first_clus = ((uint32_t)(pdir[count].DIR_FstClusHI) << 16) + pdir[count].DIR_FstClusLO;
    pinfo->info.st_dev = get_vol_id();
    pinfo->info.st_ino = pinfo->first_clus;
    pinfo->info.st_mode = ((pdir[count].DIR_Attr & 0x10) ? (0555 | S_IFDIR) : (0777 | S_IFREG));
    pinfo->info.st_nlink = (pdir[count].DIR_Attr & 0x10) ? 2 : 1;
    pinfo->info.st_uid = 0;
    pinfo->info.st_gid = 0;
    pinfo->info.st_rdev = 0;
    pinfo->info.st_size = pdir[count].DIR_FileSize;
    pinfo->info.st_blksize = block_size;
    pinfo->info.st_blocks = 0;
    auto first_clus = pinfo->first_clus;
    while (first_clus >= 2 && first_clus < count_of_cluster + 2)
    {
        ++(pinfo->info.st_blocks);
        first_clus = get_fat(first_clus);
    }
    struct tm broken_time;
    memset(&broken_time, 0, sizeof(struct tm));
    broken_time.tm_year = (pdir[count].DIR_CrtDate >> 9) + 80;
    broken_time.tm_mon = ((pdir[count].DIR_CrtDate & 0x1e0) >> 5) - 1;
    broken_time.tm_mday = (pdir[count].DIR_CrtDate & 0x1f) - 1;
    broken_time.tm_hour = (pdir[count].DIR_CrtTime >> 11);
    broken_time.tm_min = ((pdir[count].DIR_CrtTime & 0x7e0) >> 5);
    broken_time.tm_sec = (pdir[count].DIR_CrtTime & 0x1f) * 2 + pdir[count].DIR_CrtTimeTenth / 100;
    pinfo->info.st_ctim.tv_sec = mktime(&broken_time);
    pinfo->info.st_ctim.tv_nsec = (pdir[count].DIR_CrtTimeTenth % 100) * 10000000;
    memset(&broken_time, 0, sizeof(struct tm));
    broken_time.tm_year = (pdir[count].DIR_WrtDate >> 9) + 80;
    broken_time.tm_mon = ((pdir[count].DIR_WrtDate & 0x1e0) >> 5) - 1;
    broken_time.tm_mday = (pdir[count].DIR_WrtDate & 0x1f) - 1;
    broken_time.tm_hour = (pdir[count].DIR_WrtTime >> 11);
    broken_time.tm_min = ((pdir[count].DIR_WrtTime & 0x7e0) >> 5);
    broken_time.tm_sec = (pdir[count].DIR_WrtTime & 0x1f) * 2;
    pinfo->info.st_mtim.tv_sec = mktime(&broken_time);
    pinfo->info.st_mtim.tv_nsec = 0;
    memset(&broken_time, 0, sizeof(struct tm));
    broken_time.tm_year = (pdir[count].DIR_LstAccDate >> 9) + 80;
    broken_time.tm_mon = ((pdir[count].DIR_LstAccDate & 0x1e0) >> 5) - 1;
    broken_time.tm_mday = (pdir[count].DIR_LstAccDate & 0x1f) - 1;
    pinfo->info.st_atim.tv_sec = mktime(&broken_time);
    pinfo->info.st_atim.tv_nsec = 0;
    return count + 1;
}

} // namespace dev_io
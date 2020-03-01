#ifndef ICONVPP_HPP
#define ICONVPP_HPP

#include <errno.h>
#include <iconv.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace iconvpp
{

class converter
{
public:
    converter(std::string_view out_encode,
              std::string_view in_encode,
              bool ignore_error = false,
              size_t buf_size = 1024)
        : ignore_error_(ignore_error),
          buf_size_(buf_size)
    {
        if (buf_size == 0)
        {
            throw std::runtime_error("buffer size must be greater than zero");
        }

        iconv_t conv = ::iconv_open(out_encode.data(), in_encode.data());
        if (conv == (iconv_t)-1)
        {
            if (errno == EINVAL)
                throw std::runtime_error(
                    std::string() + "not supported from " + in_encode.data() + " to " + out_encode.data());
            else
                throw std::runtime_error("unknown error");
        }
        iconv_ = conv;
    }

    ~converter()
    {
        iconv_close(iconv_);
    }

    std::string convert(std::string_view input) const
    {
        std::vector<char> in_buf(input.begin(), input.end());
        auto src_ptr = in_buf.data();
        auto src_size = input.size();

        std::vector<char> buf(buf_size_);
        std::string dst;
        while (src_size)
        {
            auto dst_ptr = buf.data();
            auto dst_size = buf.size();
            size_t res = ::iconv(iconv_, &src_ptr, &src_size, &dst_ptr, &dst_size);
            if (res == (size_t)-1)
            {
                if (ignore_error_)
                {
                    // skip character
                    ++src_ptr;
                    --src_size;
                }
                else
                {
                    check_convert_error();
                }
            }
            dst.append(buf.data(), buf.size() - dst_size);
        }
        return std::move(dst);
    }

private:
    static void check_convert_error()
    {
        switch (errno)
        {
        case EILSEQ:
        case EINVAL:
            throw std::runtime_error("invalid multibyte chars");
        default:
            throw std::runtime_error("unknown error");
        }
    }

    iconv_t iconv_;
    bool ignore_error_;
    const size_t buf_size_;
};

} // namespace iconvpp

#endif
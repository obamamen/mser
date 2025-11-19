/* ================================== *\
 @file     targets.hpp
 @project  mser
 @author   moosm
 @date     11/19/2025
*\ ================================== */

#ifndef MSER_TARGETS_HPP
#define MSER_TARGETS_HPP

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iosfwd>
#include <vector>
#include <algorithm>

namespace mser
{
    struct buffer_target
    {
        std::vector<char> data;
        size_t read_pos = 0;

        void write(const char* ptr, const size_t size)
        {
            data.insert(data.end(), ptr, ptr + size);
        }

        void read(char* ptr, const size_t size)
        {
            if (read_pos + size > data.size())
                throw std::runtime_error("Buffer overflow");

            std::memcpy(ptr, data.data() + read_pos, size);
            read_pos += size;
        }

        [[nodiscard]] char peek() const
        {
            if (read_pos >= data.size())
                return '\0';

            return *(data.data() + read_pos);
        }

        [[nodiscard]] bool eof() const
        {
            return read_pos >= data.size();
        }

        [[nodiscard]] size_t tell() const { return read_pos; }

        [[nodiscard]] std::string context(size_t before = 10, size_t after = 10, int offset = 0) const
        {
            if (data.empty()) return "";

            size_t center;
            if (offset < 0 && size_t(-offset) > read_pos)
                center = 0;
            else if (offset < 0)
                center = read_pos - size_t(-offset);
            else
                center = std::min(read_pos + size_t(offset), data.size());

            size_t start = (center < before) ? 0 : center - before;

            size_t total_desired = before + after;
            size_t end;
            if (start == 0)
            {
                end = std::min(total_desired, data.size());
            } else
            {
                end = std::min(center + after, data.size());
            }

            return {data.begin() + start, data.begin() + end};
        }
    };

    struct file_target
    {
        std::ofstream ofs;
        mutable std::ifstream ifs;
        mutable size_t read_pos = 0;
    public:
        explicit file_target(const std::string& filename, bool write_mode = true)
        {
            if (write_mode)
                ofs.open(filename, std::ios::binary);
            else
                ifs.open(filename, std::ios::binary);
        }

        void write(const char* ptr, const size_t size)
        {
            ofs.write(ptr, static_cast<long long>(size));
        }

        void read(char* ptr, const size_t size)
        {
            ifs.read(ptr, static_cast<long long>(size));
            read_pos += size;
        }

        char peek()
        {
            const int c = ifs.peek();
            if (c == EOF)
                throw std::runtime_error("End of stream");
            return static_cast<char>(c);
        }

        [[nodiscard]] bool eof()
        {
            const int c = ifs.peek();
            return c == EOF;
        }

        size_t tell() const
        {
            return read_pos;
        }

        [[nodiscard]] std::string context(size_t before = 10, size_t after = 10, int offset = 0) const
        {
            auto current_pos = ifs.tellg();
            if (current_pos == -1) return "";
            int current_pos_int = current_pos;
            std::streampos center_pos;
            if (offset < 0 && size_t(-offset) > size_t(current_pos))
                center_pos = 0;
            else if (offset < 0)
                center_pos = current_pos - std::streamoff(-offset);
            else
                center_pos = current_pos + std::streamoff(offset);

            std::streampos start_pos = (size_t(center_pos) < before) ?
                std::streampos(0) : center_pos - std::streamoff(before);

            std::streampos end_pos = current_pos + std::streamoff(after);

            size_t len = end_pos - start_pos;

            std::vector<char> buf(len);

            ifs.seekg(start_pos);
            ifs.read(buf.data(), len);
            size_t read_count = static_cast<size_t>(ifs.gcount());

            ifs.seekg(current_pos);

            return {buf.begin(), buf.begin() + read_count};
        }
    };

    struct buffered_file_target
    {
        std::ofstream ofs;
        mutable std::ifstream ifs;
        std::vector<char> buffer;
        size_t buffer_size;
        size_t read_pos = 0;

        explicit buffered_file_target(const std::string& filename, const size_t buf_size = 1 << 20)
            : ofs(filename, std::ios::binary)
            , ifs(filename, std::ios::binary)
            , buffer_size(buf_size)
        {
            buffer.reserve(buffer_size);
        }

        ~buffered_file_target()
        {
            flush();
        }

        void write(const char* ptr, const size_t size)
        {
            if (buffer.size() + size > buffer_size)
                flush();
            if (size > buffer_size)
                ofs.write(ptr, static_cast<long long>(size));
            else
                buffer.insert(buffer.end(), ptr, ptr + size);
        }

        void flush()
        {
            if (!buffer.empty())
            {
                ofs.write(buffer.data(), static_cast<long long>(buffer.size()));
                buffer.clear();
            }
        }

        void read(char* ptr, const size_t size)
        {
            if (!ifs.read(ptr, static_cast<long long>(size)))
                throw std::runtime_error("File read failed");
            read_pos = static_cast<size_t>(ifs.tellg());
        }

        char peek()
        {
            int c = ifs.peek();
            if (c == EOF)
                throw std::runtime_error("End of file");
            return static_cast<char>(c);
        }

        [[nodiscard]] bool eof()
        {
            const int c = ifs.peek();
            return c == EOF;
        }

        [[nodiscard]] size_t tell() const
        {
            return read_pos;
        }

        [[nodiscard]] std::string context(size_t before = 10, size_t after = 10, int offset = 0) const
        {
            auto current_pos = ifs.tellg();
            if (current_pos == -1) return "";

            std::streampos center_pos;
            if (offset < 0 && size_t(-offset) > size_t(current_pos))
                center_pos = 0;
            else if (offset < 0)
                center_pos = current_pos - std::streamoff(-offset);
            else
                center_pos = current_pos + std::streamoff(offset);


            std::streampos start_pos = (size_t(center_pos) < before) ?
                std::streampos(0) : center_pos - std::streamoff(before);

            std::streampos end_pos = current_pos + std::streamoff(after);

            size_t len = end_pos - start_pos;


            std::vector<char> buf(len);

            ifs.seekg(start_pos);
            ifs.read(buf.data(), len);
            size_t read_count = static_cast<size_t>(ifs.gcount());

            ifs.seekg(current_pos);

            return {buf.begin(), buf.begin() + read_count};
        }
    };
}

#endif //MSER_TARGETS_HPP
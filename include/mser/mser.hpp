/* ================================== *\
 @file     mser.hpp
 @project  mser
 @author   moosm
 @date     11/19/2025
*\ ================================== */

#ifndef MSER_MSER_HPP
#define MSER_MSER_HPP
#include <string>
#include <vector>

#include "type.hpp"

namespace mser
{
    struct buffer_target;

    template <typename Target>
    class serializer
    {
        Target& target;
        format_type format = format_type::text;
        bool pretty = false;
        int indent = 0;
    public:
        explicit serializer(Target& t, const format_type f, const bool p = false)
            : target(t)
            , format(f)
            , pretty(p)
        {}

        explicit serializer(Target& t)
            : target(t)
        {}

        template <typename T,
                  typename = std::enable_if_t<!std::is_same_v<T, char> &&
                                              std::is_trivially_copyable_v<T>>>
        void write(const T& value)
        {
            if (format == format_type::binary)
                target.write(reinterpret_cast<const char*>(&value), sizeof(T));
            else
            {
                std::string s = std::to_string(value) + ";";
                target.write(s.data(), s.size());
            }
        }

        void write(const char& value)
        {
            if (format == format_type::binary)
                target.write(reinterpret_cast<const char*>(&value), sizeof(const char));
            else
            {
                target.write(&value, 1);
            }
        }

        void write(const std::string& value)
        {
            if (format == format_type::binary)
            {
                size_t len = value.size();
                target.write(reinterpret_cast<const char*>(&len), sizeof(len));
                target.write(value.data(), len);
            }
            else
            {
                target.write("\"", 1);
                for (char ch : value)
                {
                    if (ch == '"' || ch == '\\')
                        target.write("\\", 1);
                    target.write(&ch, 1);
                }
                target.write("\"; ", 3);
            }
        }

        template <typename T>
        void write(const std::vector<T>& vec)
        {
            if (format == format_type::binary)
            {
                const size_t len = vec.size();
                write(len);
                for (const auto& item : vec)
                    write(item);
            }
            else
            {
                target.write("[", 1);

                if (vec.empty()) { target.write("]", 1); return; }
                if (pretty) target.write("\n", 1);

                indent++;
                for (size_t i = 0; i < vec.size(); ++i)
                {
                    if (pretty) for (int j = 0; j < indent; ++j) target.write("\t", 1);
                    write(vec[i]);
                    if (pretty) target.write("\n", 1);
                }
                indent--;

                if (pretty) for (int i = 0; i < indent; i++) target.write("\t", 1);
                target.write("]", 1);
            }
        }


        template <typename T>
        serializer& operator<<(const T& value)
        {
            write(value);
            return *this;
        }
    };

    template <typename Target>
    class deserializer
    {
        Target& target;
        format_type format = format_type::text;
    public:
        explicit deserializer(Target& t, const format_type f)
            : target(t)
            , format(f)
        {}

        explicit deserializer(Target& t)
            : target(t)
        {}

        template <typename T>
        T read()
        {
            T value;
            read(value);
            return value;
        }

        template <typename T,
                  typename = std::enable_if_t<!std::is_same_v<T, char> &&
                                              std::is_trivially_copyable_v<T>>>
        void read(T& value)
        {
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                if (format == format_type::binary)
                    target.read(reinterpret_cast<char*>(&value), sizeof(T));
                else
                {
                    static_assert(std::is_arithmetic_v<T>, "T needs to be arithmetic for text mode.");
                    _skip();
                    std::string tmp;
                    char ch;
                    int o = 0;
                    while (!target.eof())
                    {
                        ch = target.peek();
                        if (ch == ' ' || ch == ';')
                            break;

                        if ((ch < '0' || ch > '9') && (ch == '-' && o != 0))
                        {
                            _error_char(ch,"Expected a numerical.");
                        }

                        target.read(&ch, 1);
                        tmp += ch;
                        o++;
                    }

                    if constexpr (std::is_integral_v<T>)
                        value = std::stoll(tmp);
                    else
                        value = std::stod(tmp);

                    _skip();
                }
            }
            else
            {
                static_assert(sizeof(T) == 0, "No read overload for this type");
            }
        }

        void read(std::string& value)
        {
            if (format == format_type::binary)
            {
                size_t len;
                target.read(reinterpret_cast<char*>(&len), sizeof(len));
                value.resize(len);
                target.read(value.data(), len);
            }
            else
            {
                _skip();
                if (target.peek() != '"') _error_char(target.peek(),"Expected a '\"'");

                char ch;
                target.read(&ch, 1);

                value.clear();
                bool escape = false;
                while (true)
                {
                    target.read(&ch, 1);
                    if (escape)
                    {
                        value += ch;
                        escape = false;
                    }
                    else if (ch == '\\')
                    {
                        escape = true;
                    }
                    else if (ch == '"')
                    {
                        break;
                    }
                    else
                    {
                        value += ch;
                    }
                }
                _skip();
            }
        }

        void read(char& value)
        {
            if (format == format_type::binary)
            {
                target.read(&value, 1);
            }
            else
            {
                _skip();
                char ch;
                target.read(&ch, 1);
                value = ch;
                _skip();
            }
        }

        template <typename T>
        void read(std::vector<T>& vec)
        {
            _skip();
            if (target.peek() != '[') _error_char(target.peek(),"Expected '['.");

            char discard;
            target.read(&discard, 1);
            vec.clear();
            while (target.peek() != ']')
            {
                vec.push_back({});
                read(vec.back());
            }
            target.read(&discard, 1);
            _skip();
        }

        template <typename T>
        deserializer& operator>>(T& value)
        {
            read(value);
            return *this;
        }

    private:
        void _skip()
        {
            while (!target.eof())
            {
                char c = target.peek();
                if (c == ' ' || c == ';' || c == '\n' || c == '\t' || c == '\r')
                {
                    char discard;
                    target.read(&discard, 1);
                }
                else
                {
                    break;
                }
            }
        }

        void _error_char(
            char ch = '\0',
            const std::string& error_d = "",
            const std::string& error = "Unexpected character")
        {
            auto pos = target.tell();
            std::string ctx_before = target.context(10, 0, 0);
            std::string ctx_after  = target.context(0, 10, 1);
            throw std::runtime_error(
                error + " '" + std::string{ch} + "' at position " + std::to_string(pos) + ". " + error_d +"\n"
                + ctx_before + std::string{ch} + ctx_after + "\n" +
                std::string(ctx_before.size(),' ') + '|'
            );
        }
    };
}

#endif //MSER_MSER_HPP
#include <iostream>

#include "include/mser/mser.hpp"
#include "include/mser/targets.hpp"

template<typename T>
std::ostream& operator<<(
    std::ostream& os,
    const std::vector<T>& v)
{
    os << "[";
    for (size_t i = 0; i < v.size(); ++i)
    {
        os << v[i];
        if (i + 1 != v.size()) os << ", ";
    }
    os << "]";
    return os;
}

#define P(T) ds.read<T>() << std::endl

int main()
{
    {
        mser::file_target fbuff{"test.txt", true};
        std::vector<
            std::vector<
                std::vector<int>
            >
        > intys = {};

        mser::serializer s(fbuff,mser::format_type::text, true);
        s.write(intys);
    }

    try
    {
        mser::file_target fbuff{"test.txt", false};
        mser::deserializer ds(fbuff, mser::format_type::text);
        std::cout << P(std::vector<std::vector<std::vector<int>>>);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}

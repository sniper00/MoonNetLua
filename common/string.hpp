#pragma once
#include <algorithm>
#include <locale>
#include <cmath>
#include <sstream>
#include <cctype>
#include <charconv>
#include <iostream>
#include "macro_define.hpp"

namespace moon
{
    template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    inline T string_convert(const std::string_view& s, std::errc& ec)
    {
        T result;
        auto res = std::from_chars(s.data(), s.data() + s.size(), result);
        ec = res.ec;
        return result;
    }

    template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    inline T string_convert(const std::string_view& s)
    {
        T result;
        if (auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), result); ec != std::errc())
        {
            throw std::runtime_error(std::to_string(static_cast<int>(ec)));
        }
        return result;
    }

    template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    inline T string_convert(const std::string_view& s, std::errc& ec, int base = 10)
    {
        T result{};
        auto res = std::from_chars(s.data(), s.data() + s.size(), result, base);
        ec = res.ec;
        return result;
    }

    template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    inline T string_convert(const std::string_view& s, int base = 10)
    {
        T result;
        if (auto[p, ec] = std::from_chars(s.data(), s.data() + s.size(), result, base); ec != std::errc())
        {
            throw std::runtime_error(std::to_string(static_cast<int>(ec)));
        }
        return result;
    }

    template<typename T, std::enable_if_t<std::is_same_v<T, std::string>, int> = 0>
    inline std::string string_convert(const std::string_view& s)
    {
        return std::string(s);
    }

    template<typename T, std::enable_if_t<std::is_same_v<T, std::string_view>, int> = 0>
    inline std::string_view string_convert(const std::string_view& s)
    {
        return s;
    }

    constexpr uint64_t pow10(int n)
    {
        return (n < 1) ? 1 : 10 * pow10(n - 1);
    }

    inline size_t int_log10(size_t v)
    {
        size_t n = 0;
        while (v > 9)
        {
            v /= 10;
            ++n;
        }
        return n;
    }

    inline size_t int_log16(size_t v)
    {
        size_t n = 0;
        while (v > 15)
        {
            v >>= 4;
            ++n;
        }
        return n;
    }

    inline size_t uint64_to_str(uint64_t value, char *dst)
    {
        static constexpr char digits[] =
            "0001020304050607080910111213141516171819"
            "2021222324252627282930313233343536373839"
            "4041424344454647484950515253545556575859"
            "6061626364656667686970717273747576777879"
            "8081828384858687888990919293949596979899";
        const  size_t length = int_log10(value) + 1;
        size_t next = length - 1;
        while (value >= 100) {
            const int i = (value % 100) * 2;
            value /= 100;
            dst[next] = digits[i + 1];
            dst[next - 1] = digits[i];
            next -= 2;
        }
        // Handle last 1-2 digits
        if (value < 10) {
            dst[next] = '0' + static_cast<char>(uint32_t(value));
        }
        else {
            const int i = uint32_t(value) * 2;
            dst[next] = digits[i + 1];
            dst[next - 1] = digits[i];
        }
        return length;
    }

    inline size_t uint64_to_hexstr(uint64_t value, char *dst, size_t fillzero = 0)
    {
        static constexpr char digits[] =
            "000102030405060708090A0B0C0D0E0F"
            "101112131415161718191A1B1C1D1E1F"
            "202122232425262728292A2B2C2D2E2F"
            "303132333435363738393A3B3C3D3E3F"
            "404142434445464748494A4B4C4D4E4F"
            "505152535455565758595A5B5C5D5E5F"
            "606162636465666768696A6B6C6D6E6F"
            "707172737475767778797A7B7C7D7E7F"
            "808182838485868788898A8B8C8D8E8F"
            "909192939495969798999A9B9C9D9E9F"
            "A0A1A2A3A4A5A6A7A8A9AAABACADAEAF"
            "B0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF"
            "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF"
            "D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF"
            "E0E1E2E3E4E5E6E7E8E9EAEBECEDEEEF"
            "F0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF";
        const size_t length = int_log16(value) + 1;
        size_t padding = 0;
        while (length + padding < fillzero)
        {
            dst[0] = '0';
            ++dst;
            ++padding;
        }
        size_t next = length - 1;
        while (value >= 256) {
            const int i = (value % 256) * 2;
            value /= 256;
            dst[next] = digits[i + 1];
            dst[next - 1] = digits[i];
            next -= 2;
        }
        // Handle last 1-2 digits
        if (value < 10) {
            dst[next] = '0' + static_cast<char>(uint32_t(value));
        }
        else {
            const int i = uint32_t(value) * 2;
            dst[next] = digits[i + 1];
            if (next > 0)
            {
                dst[next - 1] = digits[i];
            }
        }
        return length + padding;
    }

    /*
    e. split("aa/bb/cc","/")
    */
    template<class T>
    std::vector<T> split(const std::string_view& src, const std::string_view& sep)
    {
        std::vector<T> r;
        std::string_view::const_iterator b = src.begin();
        std::string_view::const_iterator e = src.end();
        for (auto i = src.begin(); i != src.end(); ++i)
        {
            if (sep.find(*i) != std::string_view::npos)
            {
                if (b != e && b != i)
                {
                    r.push_back(string_convert<T>(std::string_view(std::addressof(*b), size_t(i - b))));
                }
                b = e;
            }
            else
            {
                if (b == e)
                {
                    b = i;
                }
            }
        }
        if (b != e) r.push_back(string_convert<T>(std::string_view(std::addressof(*b), size_t(e - b))));
        return r;
    }

    //format string
    inline std::string format(const char* fmt, ...)
    {
        if (!fmt) return std::string("");

        size_t fmt_buffer_size = 1024;

        std::string res;
        res.resize(fmt_buffer_size);

        va_list ap;
        va_start(ap, fmt);
        int len = moon_vsnprintf(res.data(), res.size(), fmt, ap);
        va_end(ap);
        if (len >= 0 && len <= static_cast<int>(res.size()))
        {
            res.resize(len);
            return res;
        }
        else
        {
            for (;;)
            {
                fmt_buffer_size *= 2;
                res.resize(fmt_buffer_size);
                va_start(ap, fmt);
                len = moon_vsnprintf(res.data(), res.size(), fmt, ap);
                va_end(ap);
                if (len < 0)
                {
                    continue;
                }
                else
                {
                    res.resize(len);
                    break;
                }
            }
        }

        if (len < 0) {
            std::cerr << "vsnprintf error :" << std::endl;
            return std::string("");
        }
        return res;
    }

    //return left n char
    inline std::string				left(const std::string &str, size_t n)
    {
        return std::string(str, 0, n);
    }

    //return right n char
    inline std::string				right(const std::string &str, size_t n)
    {
        size_t s = (str.size() >= n) ? (str.size() - n) : 0;
        return std::string(str, s);
    }

    //" /t/n/r"
    inline std::string_view trim_right(std::string_view v)
    {
        const auto words_end(v.find_last_not_of(" \t\n\r"));
        if (words_end != std::string_view::npos) {
            v.remove_suffix(v.size() - words_end - 1);
        }
        return v;
    }

    inline std::string_view trim_left(std::string_view v)
    {
        const auto words_begin(v.find_first_not_of(" \t\n\r"));
        v.remove_prefix(std::min(words_begin, v.size()));
        return v;
    }

    inline std::string_view trim(std::string_view v)
    {
        const auto words_end(v.find_last_not_of(" \t\n\r"));
        if (words_end != std::string_view::npos) {
            v.remove_suffix(v.size() - words_end - 1);
        }
        const auto words_begin(v.find_first_not_of(" \t\n\r"));
        v.remove_prefix(std::min(words_begin, v.size()));
        return v;
    }

    inline void replace(std::string& src, std::string_view old, std::string_view strnew)
    {
        for (std::string::size_type pos(0); pos != std::string::npos; pos += strnew.size()) {
            if ((pos = src.find(old, pos)) != std::string::npos)
                src.replace(pos, old.size(), strnew);
            else
                break;
        }
    }

    //https://en.cppreference.com/w/cpp/string/byte/tolower
    //the behavior of std::tolower is undefined if the argument's value is neither representable 
    //as unsigned char nor equal to EOF. 
    //To use these functions safely with plain chars (or signed chars), 
    //	the argument should first be converted to unsigned char
    inline char toupper(unsigned char c)
    {
        return static_cast<char>(std::toupper(c));
    }

    inline char tolower(unsigned char c)
    {
        return static_cast<char>(std::tolower(c));
    }

    inline void upper(std::string& src)
    {
        std::transform(src.begin(), src.end(), src.begin(), toupper);
    }

    inline void lower(std::string& src)
    {
        std::transform(src.begin(), src.end(), src.begin(), tolower);
    }

    //! case insensitive
    inline bool iequal_string_locale(const std::string&str1, const std::string& str2, const std::locale& Loc = std::locale())
    {
        if (str1.size() != str2.size())
            return false;

        auto iter1begin = str1.begin();
        auto iter2begin = str2.begin();

        auto iter1end = str1.end();
        auto iter2end = str2.end();

        for (; iter1begin != iter1end && iter2begin != iter2end; ++iter1begin, ++iter2begin)
        {
            if (!(toupper(*iter1begin, Loc) == toupper(*iter2begin, Loc)))
            {
                return false;
            }
        }
        return true;
    }

    template<typename TString>
    inline bool iequal_string(const TString&str1, const TString& str2)
    {
        if (str1.size() != str2.size())
            return false;

        auto iter1begin = str1.begin();
        auto iter2begin = str2.begin();

        auto iter1end = str1.end();
        auto iter2end = str2.end();

        for (; iter1begin != iter1end && iter2begin != iter2end; ++iter1begin, ++iter2begin)
        {
            if (!(toupper(*iter1begin) == toupper(*iter2begin)))
            {
                return false;
            }
        }
        return true;
    }

    inline std::string hex_string(std::string_view text)
    {
        static constexpr std::string_view hex = "0123456789abcdef";
        std::string res(text.size()*2, 0);
        size_t i = 0;
        for (uint8_t c : text)
        {
            res[i * 2] = hex[c >> 4];
            res[i * 2 + 1] = hex[c & 0xf];
            ++i;
        }
        return res;
    }

    template<typename TString>
    struct ihash_string_functor
    {
        size_t operator()(const TString &str) const noexcept
        {
            size_t h = 0;
            std::hash<int> hash;
            for (auto c : str)
                h ^= hash(tolower(c)) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    using ihash_string_functor_t = ihash_string_functor<std::string>;
    using ihash_string_view_functor_t = ihash_string_functor<std::string_view>;

    template<typename TString>
    struct iequal_string_functor
    {
        bool operator()(const TString &str1, const TString &str2) const noexcept
        {
            return iequal_string(str1, str2);
        }
    };

    using iequal_string_functor_t = iequal_string_functor<std::string>;
    using iequal_string_view_functor_t = iequal_string_functor<std::string_view>;
};

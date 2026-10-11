#pragma once

#include <afx.h>
#include <atlconv.h>
#include <cstddef>
#include <functional>
#include <string>

namespace rceditor
{
    // CStringA/CStringW 与 UTF-8 std::string 之间的统一转换入口。
    class CStringConverter
    {
    public:
        // 宽字符（UTF-16）转换为当前代码页的窄字符。
        static CStringA ToAnsi(const CStringW& value);

        // 当前代码页的窄字符转换为宽字符（UTF-16）。
        static CStringW ToWide(const CStringA& value);

        // 宽字符（UTF-16）转换为 UTF-8 字节串。
        static std::string ToUtf8(const CStringW& value);

        // UTF-8 字节串转换为宽字符（UTF-16）。
        static CStringW FromUtf8(const std::string& value);

        // 根据当前工程的 UNICODE/MBCS 设置转换为 CString。
        static CString ConvertToT(const CStringW& value);
        static CString ConvertToT(const CStringA& value);
    };

    // 将 UTF-8 字面量转换为当前工程使用的 CString 类型。
    CString Utf8ToCString(const char* utf8String);

    // UTF-8 字面量包装器，保留原等级编辑器的 LPCTSTR 转换行为。
    class CrossPlatformStringForLevel
    {
    public:
        explicit CrossPlatformStringForLevel(const char* utf8String);

        // UTF-8 -> 当前本地字符串编码。
        static std::string UTF8ToLocal(const char* utf8String);

        // 当前本地字符串编码 -> UTF-8。
        static std::string LocalToUTF8(const char* localString);

        LPCTSTR c_str() const { return m_value.GetString(); }
        operator LPCTSTR() const { return m_value.GetString(); }

    private:
        CString m_value;
    };

    // CString 的哈希/相等比较，用于 unordered_map 等容器。
    struct CStringHash
    {
        size_t operator()(const CStringW& value) const noexcept
        {
            const wchar_t* data = value.GetString();
            const int length = value.GetLength();
            if (data == nullptr || length <= 0)
                return 0;

            size_t hash = sizeof(size_t) == 8 ? 14695981039346656037ull : 2166136261u;
            const size_t prime = sizeof(size_t) == 8 ? 1099511628211ull : 16777619u;
            for (int i = 0; i < length; ++i)
            {
                const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&data[i]);
                for (size_t byte = 0; byte < sizeof(wchar_t); ++byte)
                {
                    hash ^= static_cast<size_t>(bytes[byte]);
                    hash *= prime;
                }
            }
            return hash;
        }

        size_t operator()(const CStringA& value) const noexcept
        {
            const char* data = value.GetString();
            const int length = value.GetLength();
            if (data == nullptr || length <= 0)
                return 0;

            size_t hash = sizeof(size_t) == 8 ? 14695981039346656037ull : 2166136261u;
            const size_t prime = sizeof(size_t) == 8 ? 1099511628211ull : 16777619u;
            for (int i = 0; i < length; ++i)
            {
                hash ^= static_cast<size_t>(static_cast<unsigned char>(data[i]));
                hash *= prime;
            }
            return hash;
        }
    };

    struct CStringEqual
    {
        bool operator()(const CStringW& left, const CStringW& right) const noexcept
        {
            return left == right;
        }

        bool operator()(const CStringA& left, const CStringA& right) const noexcept
        {
            return left == right;
        }
    };
}

namespace std
{
    template<>
    struct hash<CStringW>
    {
        size_t operator()(const CStringW& value) const noexcept
        {
            return rceditor::CStringHash()(value);
        }
    };

    template<>
    struct hash<CStringA>
    {
        size_t operator()(const CStringA& value) const noexcept
        {
            return rceditor::CStringHash()(value);
        }
    };
}


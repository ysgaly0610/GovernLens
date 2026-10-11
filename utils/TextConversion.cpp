#include "TextConversion.h"

#include <limits>

namespace rceditor
{
    CStringA CStringConverter::ToAnsi(const CStringW& value)
    {
        if (value.IsEmpty())
            return CStringA("");
#ifdef _MSC_VER
        CW2A converted(value.GetString());
        return CStringA(converted);
#else
        const int length = WideCharToMultiByte(CP_ACP, 0, value.GetString(), -1,
            nullptr, 0, nullptr, nullptr);
        if (length <= 0)
            return CStringA("");
        std::string buffer(static_cast<size_t>(length), '\0');
        WideCharToMultiByte(CP_ACP, 0, value.GetString(), -1,
            &buffer[0], length, nullptr, nullptr);
        return CStringA(buffer.c_str());
#endif
    }

    CStringW CStringConverter::ToWide(const CStringA& value)
    {
        if (value.IsEmpty())
            return CStringW(L"");
#ifdef _MSC_VER
        CA2W converted(value.GetString());
        return CStringW(converted);
#else
        const int length = MultiByteToWideChar(CP_ACP, 0, value.GetString(), -1,
            nullptr, 0);
        if (length <= 0)
            return CStringW(L"");
        std::wstring buffer(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_ACP, 0, value.GetString(), -1,
            &buffer[0], length);
        return CStringW(buffer.c_str());
#endif
    }

    std::string CStringConverter::ToUtf8(const CStringW& value)
    {
        if (value.IsEmpty())
            return std::string();
        if (value.GetLength() > (std::numeric_limits<int>::max)())
            return std::string();

        const int length = WideCharToMultiByte(CP_UTF8, 0, value.GetString(),
            value.GetLength(), nullptr, 0, nullptr, nullptr);
        if (length <= 0)
            return std::string();
        std::string result(static_cast<size_t>(length), '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, value.GetString(), value.GetLength(),
            &result[0], length, nullptr, nullptr) <= 0)
            return std::string();
        return result;
    }

    CStringW CStringConverter::FromUtf8(const std::string& value)
    {
        if (value.empty() || value.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
            return CStringW(L"");

        const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(),
            static_cast<int>(value.size()), nullptr, 0);
        if (length <= 0)
            return CStringW(L"");

        CStringW result;
        wchar_t* buffer = result.GetBuffer(length);
        if (MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            buffer, length) <= 0)
        {
            result.ReleaseBuffer(0);
            return CStringW(L"");
        }
        result.ReleaseBuffer(length);
        return result;
    }

    CString CStringConverter::ConvertToT(const CStringW& value)
    {
#ifdef UNICODE
        return CString(value);
#else
        return CString(ToAnsi(value));
#endif
    }

    CString CStringConverter::ConvertToT(const CStringA& value)
    {
#ifdef UNICODE
        return CString(ToWide(value));
#else
        return CString(value);
#endif
    }

    CString Utf8ToCString(const char* utf8String)
    {
        if (utf8String == nullptr)
            return CString();
        return CStringConverter::ConvertToT(
            CStringConverter::FromUtf8(std::string(utf8String)));
    }

    CrossPlatformStringForLevel::CrossPlatformStringForLevel(const char* utf8String)
    {
        if (utf8String == nullptr)
        {
            m_value.Empty();
            return;
        }
        m_value = Utf8ToCString(utf8String);
    }

    std::string CrossPlatformStringForLevel::UTF8ToLocal(const char* utf8String)
    {
        if (utf8String == nullptr)
            return std::string();
#ifdef UNICODE
        return std::string(utf8String);
#else
        const CStringW wide = CStringConverter::FromUtf8(std::string(utf8String));
        const CStringA local = CStringConverter::ToAnsi(wide);
        return std::string(local.GetString());
#endif
    }

    std::string CrossPlatformStringForLevel::LocalToUTF8(const char* localString)
    {
        if (localString == nullptr)
            return std::string();
#ifdef UNICODE
        return std::string(localString);
#else
        const CStringW wide = CStringConverter::ToWide(CStringA(localString));
        return CStringConverter::ToUtf8(wide);
#endif
    }
}

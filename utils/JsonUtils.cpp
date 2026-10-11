#include "JsonUtils.h"
#include "TextConversion.h"

#include <fstream>
#include <limits>

#ifdef _MSC_VER
#ifdef _DEBUG
#pragma comment(lib, "3rd/json vc71/lib/json_vc71_libmtd.lib")
#else
#pragma comment(lib, "3rd/json vc71/lib/json_vc71_libmt.lib")
#endif
#endif

namespace
{
    CStringA ToAnsiPath(const CString& path)
    {
#ifdef UNICODE
        return rceditor::CStringConverter::ToAnsi(path);
#else
        return CStringA(path);
#endif
    }
}

namespace rceditor
{
    std::string JsonUtils::CStringWToUtf8(const CStringW& value)
    {
        return CStringConverter::ToUtf8(value);
    }

    CStringW JsonUtils::Utf8ToCStringW(const std::string& value)
    {
        return CStringConverter::FromUtf8(value);
    }

    bool JsonUtils::LoadJson(const CString& path, Json::Value& outRoot)
    {
        const CStringA pathA = ToAnsiPath(path);
        if (pathA.IsEmpty())
            return false;

        std::ifstream input(pathA.GetString(), std::ios::in | std::ios::binary);
        if (!input.is_open())
            return false;

        Json::Reader reader;
        return reader.parse(input, outRoot, false);
    }

    bool JsonUtils::SaveJson(const CString& path, const Json::Value& root)
    {
        const CStringA pathA = ToAnsiPath(path);
        if (pathA.IsEmpty())
            return false;

        std::ofstream output(pathA.GetString(), std::ios::out | std::ios::binary);
        if (!output.is_open())
            return false;

        Json::StyledWriter writer;
        output << writer.write(root);
        return !output.fail();
    }

    UINT JsonUtils::SafeGetUInt(const Json::Value& parent, const char* key, UINT defaultValue)
    {
        try
        {
            const Json::Value& value = parent.get(key, Json::Value::null);
            if (value.isNull())
                return defaultValue;
            if (value.isUInt())
            {
                const Json::UInt number = value.asUInt();
                if (number > static_cast<Json::UInt>((std::numeric_limits<UINT>::max)()))
                    return defaultValue;
                return static_cast<UINT>(number);
            }
            if (value.isInt())
            {
                const int number = value.asInt();
                return number < 0 ? defaultValue : static_cast<UINT>(number);
            }
            if (value.isString())
            {
                const std::string text = value.asString();
                if (text.empty())
                    return defaultValue;
                unsigned long long number = 0;
                try
                {
                    number = std::stoull(text);
                }
                catch (...)
                {
                    return defaultValue;
                }
                if (number > (std::numeric_limits<UINT>::max)())
                    return defaultValue;
                return static_cast<UINT>(number);
            }
        }
        catch (...)
        {
        }
        return defaultValue;
    }

    int JsonUtils::SafeGetInt(const Json::Value& parent, const char* key, int defaultValue)
    {
        try
        {
            const Json::Value& value = parent.get(key, Json::Value::null);
            if (value.isNull())
                return defaultValue;
            if (value.isInt())
                return value.asInt();
            if (value.isUInt())
            {
                const Json::UInt number = value.asUInt();
                if (number > static_cast<Json::UInt>((std::numeric_limits<int>::max)()))
                    return defaultValue;
                return static_cast<int>(number);
            }
            if (value.isString())
            {
                const std::string text = value.asString();
                if (text.empty())
                    return defaultValue;
                long long number = 0;
                try
                {
                    number = std::stoll(text);
                }
                catch (...)
                {
                    return defaultValue;
                }
                if (number < (std::numeric_limits<int>::min)() ||
                    number > (std::numeric_limits<int>::max)())
                    return defaultValue;
                return static_cast<int>(number);
            }
        }
        catch (...)
        {
        }
        return defaultValue;
    }
}

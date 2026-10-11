#pragma once

#include <afx.h>
#include <string>

#include "../../AviBaseClass/3rd/json vc71/include/json.h"

namespace rceditor
{
    // 等级配置使用的 JSON 文件读写与安全数值读取工具。
    class JsonUtils
    {
    public:
        static bool LoadJson(const CString& path, Json::Value& outRoot);
        static bool SaveJson(const CString& path, const Json::Value& root);

        // 读取 JSON 中的无符号整数，拒绝负数、溢出和非法字符串。
        static UINT SafeGetUInt(const Json::Value& parent, const char* key, UINT defaultValue = 0);

        // 读取 JSON 中的有符号整数，拒绝溢出和非法字符串。
        static int SafeGetInt(const Json::Value& parent, const char* key, int defaultValue = -1);

        // 兼容旧 LevelManagerJsonHelper 调用点；实现委托给 TextConversion。
        static std::string CStringWToUtf8(const CStringW& value);
        static CStringW Utf8ToCStringW(const std::string& value);
    };

    // 兼容原等级编辑器代码中的类型名。
    using LevelManagerJsonHelper = JsonUtils;
}

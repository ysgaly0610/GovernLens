#pragma once

#include <cstddef>

namespace rceditor
{
    // 将资源缓冲区偏移量向上对齐到指定的二次幂边界。
    inline size_t AlignOffset(size_t offset, size_t alignment)
    {
        return (offset + alignment - 1) & ~(alignment - 1);
    }
}

#pragma once
#include <cstdint>

// 界面刷新控制
namespace Refresh {
    
    // 刷新标志位
    enum RefrFlag : uint32_t {
        None            = 0,
        OnShow          = 1 << 0,   // 全局：overlay show 后刷新
        MainMenu        = 1 << 1,   // 主菜单
    };
    
    extern uint32_t g_RefrFlags;
    
    // 请求刷新
    inline void RefrRequest(RefrFlag flag) {
        g_RefrFlags |= flag;
    }
    
    // 消耗刷新(自动清除)
    inline bool RefrConsume(RefrFlag flag) {
        if (g_RefrFlags & flag || g_RefrFlags & OnShow) {
            g_RefrFlags &= ~flag;
            g_RefrFlags &= ~OnShow;
            return true;
        }
        return false;
    }
    
    // 清除刷新
    inline void RefrClear(RefrFlag flag) {
        g_RefrFlags &= ~flag;
    }
    
    // 清除所有刷新
    inline void RefrClearAll() {
        g_RefrFlags = None;
    }
}

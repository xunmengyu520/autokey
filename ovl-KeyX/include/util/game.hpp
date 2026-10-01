#pragma once

#include <memory>
#include <cstring>
#include <switch.h>

/**
 * 游戏监控类 - 提供游戏相关的监控功能
 */
class GameMonitor {
public:
    /**
     * 获取当前运行程序的Title ID（任意前台app均可，取不到返回0）
     */
    static u64 getCurrentTitleId();

    /**
     * 通过Title ID获取程序名称
     * @return 如果找到游戏名称返回true，否则返回false（result中会包含"UNKNOWN"）
     */
    static bool getTitleIdGameName(u64 titleId, char* result);
};

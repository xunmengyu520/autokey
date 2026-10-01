#pragma once
#include <switch.h>

// 游戏事件类型
enum class GameEvent : uint8_t {
    Idle = 0,          // 无前台程序运行
    Running = 1,       // 程序持续运行
    Launched = 2,      // 程序启动
    Exited = 3         // 程序退出
};

// 游戏状态结果
struct GameStateResult {
    GameEvent event;
    u64 tid;
};

// 游戏监控类（静态类）
class GameMonitor {
public:
    // 检查当前前台程序状态（返回事件+TID）
    static GameStateResult GetState();
    
private:
    // 获取当前前台程序 Title ID（任意app均可，取不到返回0）
    static u64 GetCurrentGameTitleId();
    
    // 上次检测到的 TID
    static u64 m_LastTid;
};

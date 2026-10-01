#include "game.hpp"

// 静态成员初始化
u64 GameMonitor::m_LastTid = 0;

// 获取当前前台程序 Title ID（任意app均可通过，取不到返回0）
u64 GameMonitor::GetCurrentGameTitleId() {
    u64 pid = 0, tid = 0;
    
    // 1. 获取当前应用的进程ID
    if (R_FAILED(pmdmntGetApplicationProcessId(&pid))) {
        return 0;
    }

    // 2. 根据进程ID获取程序ID (Title ID)
    if (R_FAILED(pmdmntGetProgramId(&tid, pid))) {
        return 0;
    }
    
    return tid;
}

// 检查当前前台程序状态（返回事件+TID）
GameStateResult GameMonitor::GetState() {
    u64 tid = GetCurrentGameTitleId();
    
    if (tid == 0 && m_LastTid != 0) {
        m_LastTid = 0;
        return {GameEvent::Exited, 0};
    }
    
    if (tid != 0 && m_LastTid == 0) {
        m_LastTid = tid;
        return {GameEvent::Launched, tid};
    }
    
    if (tid == 0) {
        return {GameEvent::Idle, 0};
    }
    
    return {GameEvent::Running, tid};
}

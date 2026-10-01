#pragma once
#include <switch.h>

// IPC命令定义 - 与 sys-KeyX 保持一致（精简版：只保留连发）
#define CMD_ENABLE_AUTOFIRE   1   // 开启连发
#define CMD_DISABLE_AUTOFIRE  2   // 关闭连发
#define CMD_RELOAD_AUTOFIRE   6   // 重载连发配置
#define CMD_EXIT              999 // 退出系统模块

/**
 * IPC管理类 - 负责与 sys-KeyX 系统模块的通信
 */
class IPCManager {
private:
    static constexpr const char* SERVICE_NAME = "keyLoop";  // 系统模块注册的服务名
    
    Service m_service;      // IPC 服务句柄
    bool m_connected;       // 连接状态
    
    Result SendCommand(u64 cmd_id, bool auto_start = false);

public:
    IPCManager();
    ~IPCManager();
    
    IPCManager(const IPCManager&) = delete;
    IPCManager& operator=(const IPCManager&) = delete;
    
    Result connect();
    void disconnect();
    bool isConnected() const;
    
    Result sendEnableAutoFireCommand();
    Result sendDisableAutoFireCommand();
    Result sendReloadAutoFireCommand();
    Result sendExitCommand();
};

// 全局实例 - 程序退出时自动调用析构函数
extern IPCManager g_ipcManager;

#include <switch.h>
#include "app.hpp"
#include <errno.h>
#include <sys/stat.h>
#include <minIni.h>
#include <cstdlib>
#include "libnotification.h"

#define CONFIG_DIR "/config/KeyX"
#define CONFIG_PATH "/config/KeyX/config.ini"

// 检查文件是否存在
bool App::FileExists(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

// 初始化配置路径（确保配置目录存在）
bool App::InitializeConfigPath() {
    if (mkdir(CONFIG_DIR, 0777) == 0) return true;
    if (errno == EEXIST) return true;
    return false;
}

// App类的实现
App::App() {
    if (!InitializeConfigPath()) return;
    if (!InitializeIPC()) return;
    m_loop_error = false;
}

App::~App() {
}

// 初始化IPC服务
bool App::InitializeIPC() {

    // 创建IPC服务
    ipc_server = std::make_unique<IPCServer>();

    // 设置退出回调
    ipc_server->SetExitCallback([this]() {
        m_loop_error = true;
    });
    
    // 设置开启连发回调
    ipc_server->SetEnableAutoFireCallback([this]() {
        m_CurrentAutoEnable = true;
        if (m_GameInFocus) {
            if (autokey_loop) UpdateTurboConfig();
            else StartAutoKey();
        }
    });
    
    // 设置关闭连发回调
    ipc_server->SetDisableAutoFireCallback([this]() {
        m_CurrentAutoEnable = false;
        StopAutoKey();
    });
    
    // 设置重载连发配置回调
    ipc_server->SetReloadAutoFireCallback([this]() {
        UpdateTurboConfig();
    });

    // 启动服务
    if (!ipc_server->Start("keyLoop")) {
        ipc_server.reset();
        return false;
    }

    return true;
}


void App::Loop() {
    while (!m_loop_error) {
        GameStateResult game = GameMonitor::GetState();
        switch (game.event) {
            case GameEvent::Idle:
                // 无前台程序运行
                for (int i = 0; i < 10 && !m_loop_error; ++i) svcSleepThread(100000000ULL);
                continue;
            case GameEvent::Running:
                OnGameRunning(game.tid);
                if (autokey_loop) svcSleepThread(100000000ULL);
                else for (int i = 0; i < 5 && !m_loop_error; ++i) svcSleepThread(100000000ULL);
                continue;
            case GameEvent::Launched:
                OnGameLaunched(game.tid);
                break;
            case GameEvent::Exited:
                OnGameExited();
                break;
            default:
                break;
        }
        svcSleepThread(100000000ULL);  // 100ms
    }
}

// 处理前台程序启动事件
void App::OnGameLaunched(u64 tid) {
    m_FirstLaunch = true;
    m_GameInFocus = true;
    m_CurrentTid = tid;
    LoadGameConfig(tid);
    if (m_CurrentAutoEnable) StartAutoKey();
    CreateNotification(true);
}

// 处理前台程序运行事件
void App::OnGameRunning(u64 tid) {
    // 获取焦点状态(只有在焦点变化的时候才会获取到在焦点或者不在，不然获取的是无变化)
    FocusState focus = FocusMonitor::GetState(tid);
    switch (focus) {
        case FocusState::InFocus:
            m_GameInFocus = true;
            if (m_CurrentAutoEnable && autokey_loop) ResumeAutoKey();
            else if (m_CurrentAutoEnable && !autokey_loop) StartAutoKey();
            else if (!m_CurrentAutoEnable && autokey_loop) StopAutoKey();
            break;
        case FocusState::OutOfFocus:
            m_GameInFocus = false;
            if (autokey_loop) PauseAutoKey();
            break;
        default:
            break;
    }
}

// 处理前台程序退出事件
void App::OnGameExited() {
    m_GameInFocus = false;
    if (autokey_loop) StopAutoKey();
    m_CurrentTid = 0;
    CreateNotification(false);
}

// 加载游戏配置（读取并缓存配置参数）
void App::LoadGameConfig(u64 tid) {
    LoadBasicConfig(tid);
}

// 加载基础配置（确定配置路径）
void App::LoadBasicConfig(u64 tid) {
    m_notifEnabled = ini_getbool("NOTIFICATION", "notif", false, CONFIG_PATH);
    snprintf(m_GameConfigPath, sizeof(m_GameConfigPath), "/config/KeyX/GameConfig/%016lX.ini", tid);
    m_CurrentGlobConfig = ini_getbool("AUTOFIRE", "globconfig", 1, m_GameConfigPath);
    if (m_FirstLaunch && m_CurrentGlobConfig) {
        bool defaultAutoEnable = ini_getbool("AUTOFIRE", "defaultautoenable", 0, CONFIG_PATH);
        ini_putl("AUTOFIRE", "autoenable", defaultAutoEnable, CONFIG_PATH);
    }
    m_FirstLaunch = false;
    // 此处用来设定是否使用全局配置，还是独立配置
    m_ConfigPath = m_CurrentGlobConfig ? CONFIG_PATH : m_GameConfigPath;
    const char* switchConfigPath = m_CurrentGlobConfig ? CONFIG_PATH : m_GameConfigPath;
    m_CurrentAutoEnable = ini_getbool("AUTOFIRE", "autoenable", 0, switchConfigPath);
}

// 开启按键模块
bool App::StartAutoKey() {
    std::lock_guard<std::mutex> lock(autokey_mutex);
    // 如果已经创建，则不重复创建
    if (autokey_loop) return true;
    autokey_loop = std::make_unique<AutoKeyLoop>(m_ConfigPath, m_CurrentAutoEnable);
    return true;
}

// 退出按键模块
void App::StopAutoKey() {
    std::lock_guard<std::mutex> lock(autokey_mutex);
    if (autokey_loop) autokey_loop.reset();
}

// 暂停连发
void App::PauseAutoKey() {
    std::lock_guard<std::mutex> lock(autokey_mutex);
    if (autokey_loop) autokey_loop->Pause();
}

// 恢复连发
void App::ResumeAutoKey() {
    std::lock_guard<std::mutex> lock(autokey_mutex);
    if (autokey_loop) autokey_loop->Resume();
}

// 更新连发配置
void App::UpdateTurboConfig() {
    std::lock_guard<std::mutex> lock(autokey_mutex);
    if (autokey_loop) {
        autokey_loop->UpdateTurboFeature(m_CurrentAutoEnable, m_ConfigPath);
    }
}

void App::CreateNotification(bool Enable) {
    if (!m_notifEnabled) return;
    if (!m_CurrentAutoEnable) return;
    const char* message = Enable ? "连发已开启" : "连发已关闭";
    createNotification(message, 2, INFO, RIGHT);
}

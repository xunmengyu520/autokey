#pragma once
#include <tesla.hpp>

// 状态结构体
struct KeyXInfo {
    bool isInGame;                      // 是否检测到前台程序
    char gameId[17];                    // 程序ID(TID)
    bool isGlobalConfig;                // 全局配置还是独立配置
    bool isAutoFireEnabled;             // 连发总开关（自动启动）
    std::string GameConfigPath;         // 独立配置文件路径
};

// 主菜单类定义（直接就是连发设置页面，不再有子菜单）
class MainMenu : public tsl::Gui 
{
public:
    MainMenu();  // 构造函数

    virtual tsl::elm::Element* createUI() override;  // 创建用户界面
    virtual void update() override;

private:

    KeyXInfo m_KeyXinfo{};
    tsl::elm::ListItem* m_AutoFireEnableItem = nullptr;

    // 8个按键各自的连发速度档位：0=关闭, 1=极速, 2=高速, 3=普通
    static constexpr int BUTTON_COUNT = 8;
    int m_Levels[BUTTON_COUNT];

    int m_ToggleIdx;      // 连发开关键在 TOGGLE_KEYS 表中的序号（0=关闭该功能）
    bool m_DelayStart;    // 防止误触（延迟启动）
    bool m_DefaultAuto;   // 默认连发（仅全局配置下新游戏首次启动时生效）
    bool m_JCRightHand;   // 单边JoyCon时，是否限制为右手柄连发

    void RefreshData();     // 更新数据（含读取各项配置）
    void AutoKeyToggle();   // 连发总开关
    void ConfigToggle();    // 配置切换（全局/独立）
};

#include "main_menu.hpp"
#include "game.hpp"
#include "ini_helper.hpp"
#include "ipc.hpp"
#include "sysmodule.hpp"
#include "hiddata.hpp"
#include "refresh.hpp"

// 全局配置文件路径
static constexpr const char* CONFIG_PATH = "/config/KeyX/config.ini";

static constexpr const char* s_switchText[] = {"已关闭", "已开启"};
static const tsl::Color s_switchColor[] = {tsl::Color(0xF, 0x7, 0x7, 0xF), tsl::onTextColor};

namespace {
    // 8个可单独配置的按键槽位，顺序固定：A,B,X,Y,L,R,ZL,ZR
    // 必须和 sys-KeyX/source/autokey/turbo.cpp 里的 CHANNEL_MASKS / CHANNEL_INI_KEYS 顺序完全一致
    struct ButtonSlot {
        const char* displayName;
        const char* iniKey;
    };
    constexpr ButtonSlot BUTTON_SLOTS[] = {
        {"A 键",  "level_a"},
        {"B 键",  "level_b"},
        {"X 键",  "level_x"},
        {"Y 键",  "level_y"},
        {"L 键",  "level_l"},
        {"R 键",  "level_r"},
        {"ZL 键", "level_zl"},
        {"ZR 键", "level_zr"},
    };
    constexpr int BUTTON_SLOT_COUNT = sizeof(BUTTON_SLOTS) / sizeof(BUTTON_SLOTS[0]);

    // 速度档位：0=关闭, 1=极速, 2=高速, 3=普通
    // 数值(ms)需要和 sys-KeyX 端 turbo.cpp 的 LevelToMs() 保持一致，这里只用来显示名字/颜色
    struct LevelInfo {
        const char* name;
        tsl::Color color;
    };
    constexpr LevelInfo SPEED_LEVELS[] = {
        {"关闭", tsl::Color(0x8, 0x8, 0x8, 0xF)},                // 灰色
        {"极速", tsl::Color(0xF, 0x5, 0x5, 0xF)},                // 红色
        {"高速", tsl::Color(0x0, 0xD, 0xF, 0xF)},                // 蓝色
        {"普通", tsl::Color(0x0, 0xF, 0xD, 0xF)},                // 标准色
    };
    constexpr int SPEED_LEVEL_COUNT = sizeof(SPEED_LEVELS) / sizeof(SPEED_LEVELS[0]);

    // 连发开关键可选项：按A依次循环切换
    struct ToggleKeyOption {
        const char* name;
        u64 mask;
    };
    constexpr ToggleKeyOption TOGGLE_KEYS[] = {
        {"关闭", 0},
        {"ZL", BTN_ZL},
        {"ZR", BTN_ZR},
        {"L", BTN_L},
        {"R", BTN_R},
        {"左摇杆按下", BTN_STICKL},
        {"右摇杆按下", BTN_STICKR},
        {"-键", BTN_SELECT},
        {"+键", BTN_START},
    };
    constexpr int TOGGLE_KEY_COUNT = sizeof(TOGGLE_KEYS) / sizeof(TOGGLE_KEYS[0]);
}

void MainMenu::RefreshData() {
    u64 currentTitleId = GameMonitor::getCurrentTitleId();
    m_KeyXinfo.isInGame = (currentTitleId != 0) && SysModuleManager::isRunning();
    snprintf(m_KeyXinfo.gameId, sizeof(m_KeyXinfo.gameId), "%016lX", currentTitleId);
    m_KeyXinfo.GameConfigPath = "/config/KeyX/GameConfig/" + std::string(m_KeyXinfo.gameId) + ".ini";
    m_KeyXinfo.isGlobalConfig = IniHelper::getBool("AUTOFIRE", "globconfig", true, m_KeyXinfo.GameConfigPath);
    std::string SwitchConfigPath = m_KeyXinfo.isGlobalConfig ? CONFIG_PATH : m_KeyXinfo.GameConfigPath;
    if (m_KeyXinfo.isInGame) {
        m_KeyXinfo.isAutoFireEnabled = IniHelper::getBool("AUTOFIRE", "autoenable", false, SwitchConfigPath);
    } else {
        m_KeyXinfo.isAutoFireEnabled = false;
    }
    if (m_AutoFireEnableItem != nullptr) {
        m_AutoFireEnableItem->setValue(s_switchText[m_KeyXinfo.isAutoFireEnabled]);
        m_AutoFireEnableItem->setValueColor(s_switchColor[m_KeyXinfo.isAutoFireEnabled]);
    }

    // 当前生效的参数配置路径（全局 or 独立）
    std::string ConfigPath = m_KeyXinfo.isGlobalConfig ? CONFIG_PATH : m_KeyXinfo.GameConfigPath;

    for (int i = 0; i < BUTTON_SLOT_COUNT; i++) {
        int level = IniHelper::getInt("AUTOFIRE", BUTTON_SLOTS[i].iniKey, 0, ConfigPath);
        if (level < 0 || level >= SPEED_LEVEL_COUNT) level = 0;
        m_Levels[i] = level;
    }

    u64 toggleMask = static_cast<u64>(IniHelper::getInt("AUTOFIRE", "togglebutton", 0, ConfigPath));
    m_ToggleIdx = 0;
    for (int i = 0; i < TOGGLE_KEY_COUNT; i++) {
        if (TOGGLE_KEYS[i].mask == toggleMask) { m_ToggleIdx = i; break; }
    }

    m_DelayStart = IniHelper::getBool("AUTOFIRE", "delaystart", true, ConfigPath);
    m_DefaultAuto = IniHelper::getBool("AUTOFIRE", "defaultautoenable", false, CONFIG_PATH);
    m_JCRightHand = IniHelper::getBool("AUTOFIRE", "IsJCRightHand", true, CONFIG_PATH);
}

// 连发总开关
void MainMenu::AutoKeyToggle() {
    if (!m_KeyXinfo.isInGame) return;
    Result rc = m_KeyXinfo.isAutoFireEnabled ? g_ipcManager.sendDisableAutoFireCommand() : g_ipcManager.sendEnableAutoFireCommand();
    if (R_FAILED(rc)) return;
    m_KeyXinfo.isAutoFireEnabled = !m_KeyXinfo.isAutoFireEnabled;
    m_AutoFireEnableItem->setValue(s_switchText[m_KeyXinfo.isAutoFireEnabled]);
    m_AutoFireEnableItem->setValueColor(s_switchColor[m_KeyXinfo.isAutoFireEnabled]);
    IniHelper::setBool("AUTOFIRE", "globconfig", m_KeyXinfo.isGlobalConfig, m_KeyXinfo.GameConfigPath);
    IniHelper::setBool("AUTOFIRE", "autoenable", m_KeyXinfo.isAutoFireEnabled, m_KeyXinfo.GameConfigPath);
    IniHelper::setBool("AUTOFIRE", "autoenable", m_KeyXinfo.isAutoFireEnabled, CONFIG_PATH);
}

// 配置切换（全局/独立）
void MainMenu::ConfigToggle() {
    if (!m_KeyXinfo.isInGame) return;
    m_KeyXinfo.isGlobalConfig = !m_KeyXinfo.isGlobalConfig;
    IniHelper::setBool("AUTOFIRE", "globconfig", m_KeyXinfo.isGlobalConfig, m_KeyXinfo.GameConfigPath);
    IniHelper::setBool("AUTOFIRE", "autoenable", m_KeyXinfo.isAutoFireEnabled, m_KeyXinfo.GameConfigPath);
    IniHelper::setBool("AUTOFIRE", "autoenable", m_KeyXinfo.isAutoFireEnabled, CONFIG_PATH);
    RefreshData();
    Result rc = g_ipcManager.sendReloadAutoFireCommand();
    if (R_FAILED(rc)) return;
}

// 主菜单构造函数
MainMenu::MainMenu()
{
    RefreshData();
}

// 创建用户界面
tsl::elm::Element* MainMenu::createUI()
{
    const char* subtitle = m_KeyXinfo.isInGame
        ? (m_KeyXinfo.isGlobalConfig ? "全局配置" : "独立配置")
        : (SysModuleManager::isRunning() ? "未检测到前台程序" : "系统模块未启动");

    auto frame = new tsl::elm::OverlayFrame("KeyX 连发", subtitle);
    auto list = new tsl::elm::List();

    // 连发总开关
    m_AutoFireEnableItem = new tsl::elm::ListItem("连发总开关", s_switchText[m_KeyXinfo.isAutoFireEnabled]);
    m_AutoFireEnableItem->setValueColor(s_switchColor[m_KeyXinfo.isAutoFireEnabled]);
    m_AutoFireEnableItem->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            AutoKeyToggle();
            return true;
        }
        return false;
    });
    list->addItem(m_AutoFireEnableItem);

    list->addItem(new tsl::elm::CategoryHeader(" 逐键设置连发速度（关闭/极速/高速/普通）"));

    for (int i = 0; i < BUTTON_SLOT_COUNT; i++) {
        const auto& slot = BUTTON_SLOTS[i];
        auto& lvl = SPEED_LEVELS[m_Levels[i]];
        auto item = new tsl::elm::ListItem(slot.displayName, lvl.name);
        item->setValueColor(lvl.color);
        item->setClickListener([this, item, i](u64 keys) {
            if (keys & HidNpadButton_A) {
                std::string ConfigPath = m_KeyXinfo.isGlobalConfig ? CONFIG_PATH : m_KeyXinfo.GameConfigPath;
                m_Levels[i] = (m_Levels[i] + 1) % SPEED_LEVEL_COUNT;
                IniHelper::setInt("AUTOFIRE", BUTTON_SLOTS[i].iniKey, m_Levels[i], ConfigPath);
                g_ipcManager.sendReloadAutoFireCommand();
                auto& newLvl = SPEED_LEVELS[m_Levels[i]];
                item->setValue(newLvl.name);
                item->setValueColor(newLvl.color);
                return true;
            }
            return false;
        });
        list->addItem(item);
    }

    list->addItem(new tsl::elm::CategoryHeader(" 连发开关键（游戏内按一下开/关全部连发）"));

    auto listItemToggleKey = new tsl::elm::ListItem("连发开关键", TOGGLE_KEYS[m_ToggleIdx].name);
    listItemToggleKey->setClickListener([this, listItemToggleKey](u64 keys) {
        if (keys & HidNpadButton_A) {
            std::string ConfigPath = m_KeyXinfo.isGlobalConfig ? CONFIG_PATH : m_KeyXinfo.GameConfigPath;
            m_ToggleIdx = (m_ToggleIdx + 1) % TOGGLE_KEY_COUNT;
            IniHelper::setInt("AUTOFIRE", "togglebutton", static_cast<int>(TOGGLE_KEYS[m_ToggleIdx].mask), ConfigPath);
            g_ipcManager.sendReloadAutoFireCommand();
            listItemToggleKey->setValue(TOGGLE_KEYS[m_ToggleIdx].name);
            return true;
        }
        return false;
    });
    list->addItem(listItemToggleKey);

    list->addItem(new tsl::elm::CategoryHeader(" 其他设置"));

    auto listItemDelayStart = new tsl::elm::ListItem("防止误触", m_DelayStart ? "开" : "关");
    listItemDelayStart->setClickListener([this, listItemDelayStart](u64 keys) {
        if (keys & HidNpadButton_A) {
            std::string ConfigPath = m_KeyXinfo.isGlobalConfig ? CONFIG_PATH : m_KeyXinfo.GameConfigPath;
            m_DelayStart = !m_DelayStart;
            IniHelper::setBool("AUTOFIRE", "delaystart", m_DelayStart, ConfigPath);
            g_ipcManager.sendReloadAutoFireCommand();
            listItemDelayStart->setValue(m_DelayStart ? "开" : "关");
            return true;
        }
        return false;
    });
    list->addItem(listItemDelayStart);

    auto listItemDefaultAuto = new tsl::elm::ListItem("默认连发（新游戏首次启动）", m_DefaultAuto ? "开" : "关");
    listItemDefaultAuto->setClickListener([this, listItemDefaultAuto](u64 keys) {
        if (keys & HidNpadButton_A) {
            m_DefaultAuto = !m_DefaultAuto;
            IniHelper::setBool("AUTOFIRE", "defaultautoenable", m_DefaultAuto, CONFIG_PATH);
            listItemDefaultAuto->setValue(m_DefaultAuto ? "开" : "关");
            return true;
        }
        return false;
    });
    list->addItem(listItemDefaultAuto);

    auto listItemJCHand = new tsl::elm::ListItem("单个JoyCon时限制右手柄", m_JCRightHand ? "开" : "关");
    listItemJCHand->setClickListener([this, listItemJCHand](u64 keys) {
        if (keys & HidNpadButton_A) {
            m_JCRightHand = !m_JCRightHand;
            IniHelper::setBool("AUTOFIRE", "IsJCRightHand", m_JCRightHand, CONFIG_PATH);
            g_ipcManager.sendReloadAutoFireCommand();
            listItemJCHand->setValue(m_JCRightHand ? "开" : "关");
            return true;
        }
        return false;
    });
    list->addItem(listItemJCHand);

    auto ConfigSwitchItem = new tsl::elm::ListItem("切换配置", m_KeyXinfo.isGlobalConfig ? "全局配置" : "独立配置");
    ConfigSwitchItem->setClickListener([this, ConfigSwitchItem](u64 keys) {
        if (keys & HidNpadButton_A) {
            ConfigToggle();
            ConfigSwitchItem->setValue(m_KeyXinfo.isGlobalConfig ? "全局配置" : "独立配置");
            return true;
        }
        return false;
    });
    list->addItem(ConfigSwitchItem);

    frame->setContent(list);
    return frame;
}

void MainMenu::update() {
    if (Refresh::RefrConsume(Refresh::MainMenu)) RefreshData();
}

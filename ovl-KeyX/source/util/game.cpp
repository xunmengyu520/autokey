#include "game.hpp"
#include "language.hpp"

// 获取当前运行程序的Title ID
u64 GameMonitor::getCurrentTitleId() {
    u64 pid = 0, tid = 0;
    
    if (R_FAILED(pmdmntGetApplicationProcessId(&pid)))
        return 0;  
    
    if (R_FAILED(pmdmntGetProgramId(&tid, pid)))
        return 0;  
    
    // 不过滤类型，任意前台应用（游戏、模拟器、其他homebrew app等）均可通过
    return tid;
}

// 根据Title ID获取游戏名称
bool GameMonitor::getTitleIdGameName(u64 titleId, char* result) {
    
    strcpy(result, "UNKNOWN");
    
    auto control_data = std::make_unique<NsApplicationControlData>();
    u64 jpeg_size{};
    
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, titleId, control_data.get(), sizeof(NsApplicationControlData), &jpeg_size);
    if (R_FAILED(rc)) {
        return false;
    }
    
    NacpLanguageEntry* entry = nullptr;
    
    int systemLanguageIndex = 0;
    switch (g_systemLanguage) {
        case SetLanguage_ENUS: systemLanguageIndex = 0; break;
        case SetLanguage_ENGB: systemLanguageIndex = 1; break;
        case SetLanguage_JA: systemLanguageIndex = 2; break;
        case SetLanguage_FR: systemLanguageIndex = 3; break;
        case SetLanguage_DE: systemLanguageIndex = 4; break;
        case SetLanguage_ES419: systemLanguageIndex = 5; break;
        case SetLanguage_ES: systemLanguageIndex = 6; break;
        case SetLanguage_IT: systemLanguageIndex = 7; break;
        case SetLanguage_NL: systemLanguageIndex = 8; break;
        case SetLanguage_FRCA: systemLanguageIndex = 9; break;
        case SetLanguage_PT: systemLanguageIndex = 10; break;
        case SetLanguage_RU: systemLanguageIndex = 11; break;
        case SetLanguage_KO: systemLanguageIndex = 12; break;
        case SetLanguage_ZHTW:
        case SetLanguage_ZHHANT: systemLanguageIndex = 13; break;
        case SetLanguage_ZHCN:
        case SetLanguage_ZHHANS: systemLanguageIndex = 14; break;
        case SetLanguage_PTBR: systemLanguageIndex = 15; break;
        default: systemLanguageIndex = 0;
    }

    entry = &control_data->nacp.lang[systemLanguageIndex];
    if (entry->name[0] != '\0'){
        strncpy(result, entry->name, 63);
        result[63] = '\0';
        return true;
    }
    
    for (int i = 0; i < 16; i++) {
        if (i == systemLanguageIndex) continue; 
        entry = &control_data->nacp.lang[i];
        if (entry->name[0] == '\0') continue;
        strncpy(result, entry->name, 63);
        result[63] = '\0';
        return true;
    }

    return false;
}

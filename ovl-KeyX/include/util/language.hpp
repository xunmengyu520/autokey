#pragma once
#include <switch.h>

// 全局变量：系统语言（用于 game.cpp 获取本地化游戏名称）
extern SetLanguage g_systemLanguage;

// 初始化系统语言（只用于获取游戏名称的本地化显示，界面文字固定为中文）
void InitSystemLanguage();

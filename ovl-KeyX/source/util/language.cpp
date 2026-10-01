#include "language.hpp"

SetLanguage g_systemLanguage = SetLanguage_ENUS;

void InitSystemLanguage() {
    u64 languageCode;
    if (R_FAILED(setGetSystemLanguage(&languageCode))) return;
    setMakeLanguage(languageCode, &g_systemLanguage);
}

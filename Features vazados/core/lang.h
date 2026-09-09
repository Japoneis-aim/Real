#pragma once
#include <Windows.h>

namespace Lang {

    enum LangID { PT_BR, EN };

    inline LangID CurrentLang = [] {
        LANGID lid = GetUserDefaultUILanguage();
        if (lid == 0x0416 || (lid & 0xFF) == 0x16)
            return PT_BR;
        return EN;
    }();

    inline const char* L(const char* en, const char* pt) {
        return CurrentLang == PT_BR ? pt : en;
    }

    inline bool ToggleLang() {
        CurrentLang = (CurrentLang == EN) ? PT_BR : EN;
        return CurrentLang == PT_BR;
    }

}

#define L(en, pt) Lang::L(en, pt)

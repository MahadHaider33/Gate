#pragma once
#include "audio/Processing.h"
#include <string>
namespace gate {
struct Preferences {
    Parameters processing;
    std::wstring microphone, listener;
    bool trayExplained = false;
    static Preferences load();
    void save() const;
};
}

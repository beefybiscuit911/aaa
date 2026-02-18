#pragma once
#include "includes.h"  // Includes windows.h with proper Winsock order
#include <string>
#include <map>

namespace KeyUtils {
    inline bool IsKeyPressed(int vkCode) {
        return (GetAsyncKeyState(vkCode) & 0x8000) != 0;
    }

    inline int GetKeyCode(const std::string& keyName) {
        static std::map<std::string, int> keyMap = {
            {"left_alt", VK_LMENU},
            {"right_alt", VK_RMENU},
            {"left_shift", VK_LSHIFT},
            {"right_shift", VK_RSHIFT},
            {"left_ctrl", VK_LCONTROL},
            {"right_ctrl", VK_RCONTROL},
            {"space", VK_SPACE},
            {"tab", VK_TAB},
            {"caps_lock", VK_CAPITAL},
            {"f1", VK_F1},
            {"f2", VK_F2},
            {"f3", VK_F3},
            {"f4", VK_F4},
            {"f5", VK_F5},
            {"f6", VK_F6},
            {"f7", VK_F7},
            {"f8", VK_F8},
            {"f9", VK_F9},
            {"f10", VK_F10},
            {"f11", VK_F11},
            {"f12", VK_F12},
            {"mouse_left", VK_LBUTTON},
            {"mouse_right", VK_RBUTTON},
            {"mouse_middle", VK_MBUTTON}
        };

        auto it = keyMap.find(keyName);
        if (it != keyMap.end()) return it->second;

        // For letter keys (a-z)
        if (keyName.length() == 1 && isalpha(keyName[0])) {
            return toupper(keyName[0]);
        }

        // For number keys (0-9)
        if (keyName.length() == 1 && isdigit(keyName[0])) {
            return keyName[0];
        }

        return VK_LMENU; // Default to left Alt
    }

    inline std::string GetKeyName(int vkCode) {
        switch (vkCode) {
        case VK_LMENU: return "left_alt";
        case VK_RMENU: return "right_alt";
        case VK_LSHIFT: return "left_shift";
        case VK_RSHIFT: return "right_shift";
        case VK_LCONTROL: return "left_ctrl";
        case VK_RCONTROL: return "right_ctrl";
        case VK_SPACE: return "space";
        case VK_TAB: return "tab";
        case VK_CAPITAL: return "caps_lock";
        case VK_F1: return "f1";
        case VK_F2: return "f2";
        case VK_F3: return "f3";
        case VK_F4: return "f4";
        case VK_F5: return "f5";
        case VK_F6: return "f6";
        case VK_F7: return "f7";
        case VK_F8: return "f8";
        case VK_F9: return "f9";
        case VK_F10: return "f10";
        case VK_F11: return "f11";
        case VK_F12: return "f12";
        case VK_LBUTTON: return "mouse_left";
        case VK_RBUTTON: return "mouse_right";
        case VK_MBUTTON: return "mouse_middle";
        default:
            if (vkCode >= 'A' && vkCode <= 'Z') return std::string(1, tolower(vkCode));
            if (vkCode >= '0' && vkCode <= '9') return std::string(1, vkCode);
            return "unknown";
        }
    }

    inline bool isValidKey(const std::string& keyName) {
        // Check if key exists in keyMap
        static std::map<std::string, int> keyMap = {
            {"left_alt", VK_LMENU},
            {"right_alt", VK_RMENU},
            {"left_shift", VK_LSHIFT},
            {"right_shift", VK_RSHIFT},
            {"left_ctrl", VK_LCONTROL},
            {"right_ctrl", VK_RCONTROL},
            {"space", VK_SPACE},
            {"tab", VK_TAB},
            {"caps_lock", VK_CAPITAL},
            {"f1", VK_F1},
            {"f2", VK_F2},
            {"f3", VK_F3},
            {"f4", VK_F4},
            {"f5", VK_F5},
            {"f6", VK_F6},
            {"f7", VK_F7},
            {"f8", VK_F8},
            {"f9", VK_F9},
            {"f10", VK_F10},
            {"f11", VK_F11},
            {"f12", VK_F12},
            {"mouse_left", VK_LBUTTON},
            {"mouse_right", VK_RBUTTON},
            {"mouse_middle", VK_MBUTTON},
            {"left_mouse_button", VK_LBUTTON},
            {"right_mouse_button", VK_RBUTTON},
            {"x1", VK_XBUTTON1},
            {"x2", VK_XBUTTON2},
            {"num_0", VK_NUMPAD0},
            {"num_1", VK_NUMPAD1},
            {"num_2", VK_NUMPAD2},
            {"num_3", VK_NUMPAD3},
            {"num_4", VK_NUMPAD4},
            {"num_5", VK_NUMPAD5},
            {"num_6", VK_NUMPAD6},
            {"num_7", VK_NUMPAD7},
            {"num_8", VK_NUMPAD8},
            {"num_9", VK_NUMPAD9},
            {"up_arrow", VK_UP},
            {"down_arrow", VK_DOWN},
            {"enter", VK_RETURN},
            {"esc", VK_ESCAPE},
            {"home", VK_HOME},
            {"insert", VK_INSERT},
            {"page_down", VK_NEXT},
            {"page_up", VK_PRIOR},
            {"backspace", VK_BACK}
        };

        if (keyMap.find(keyName) != keyMap.end()) {
            return true;
        }

        // Check for single letter (a-z)
        if (keyName.length() == 1 && isalpha(keyName[0])) {
            return true;
        }

        // Check for single digit (0-9)
        if (keyName.length() == 1 && isdigit(keyName[0])) {
            return true;
        }

        return false;
    }
}
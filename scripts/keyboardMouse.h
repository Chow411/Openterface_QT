/*
* ========================================================================== *
*                                                                            *
*    This file is part of the Openterface Mini KVM App QT version            *
*                                                                            *
*    Copyright (C) 2024   <info@openterface.com>                             *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation version 3.                                 *
*                                                                            *
*    This program is distributed in the hope that it will be useful, but     *
*    WITHOUT ANY WARRANTY; without even the implied warranty of              *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU        *
*    General Public License for more details.                                *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see <http://www.gnu.org/licenses/>.    *
*                                                                            *
*    This program incorporates portions of the AutoHotkey source code,       *
*    which is licensed under the GNU General Public License version 2 or     *
*    later. The original AutoHotkey source code can be found at:             *
*    https://github.com/Lexikos/AutoHotkey_L                                 *
*                                                                            *
*                                                                            *
*    The AutoHotkey source code is copyrighted by the AutoHotkey             *
*    development team and contributors.                                      *
*                                                                            *
* ========================================================================== *
*/



#ifndef KEYBOARDMOUSE_H
#define KEYBOARDMOUSE_H

#include <QMap>
const QMap<QString, int> AHKmapping = {
    {"a", 0x04}, // a
    {"b", 0x05}, // b
    {"c", 0x06}, // c
    {"d", 0x07}, // d
    {"e", 0x08}, // e
    {"f", 0x09}, // f
    {"g", 0x0A}, // g
    {"h", 0x0B}, // h
    {"i", 0x0C}, // i
    {"j", 0x0D}, // j
    {"k", 0x0E}, // k
    {"l", 0x0F}, // l
    {"m", 0x10}, // m
    {"n", 0x11}, // n
    {"o", 0x12}, // o
    {"p", 0x13}, // p
    {"q", 0x14}, // q
    {"r", 0x15}, // r
    {"s", 0x16}, // s
    {"t", 0x17}, // t
    {"u", 0x18}, // u
    {"v", 0x19}, // v
    {"w", 0x1A}, // w
    {"x", 0x1B}, // x
    {"y", 0x1C}, // y
    {"z", 0x1D},  // z
    {"0", 0x27}, // 0
    {"1", 0x1E}, // 1
    {"2", 0x1F}, // 2
    {"3", 0x20}, // 3
    {"4", 0x21}, // 4
    {"5", 0x22}, // 5
    {"6", 0x23}, // 6
    {"7", 0x24}, // 7
    {"8", 0x25}, // 8
    {"9", 0x26}, // 9
    {"NumberpadEnter", 0x58}, // NumberpadEnter
    {"NumpadSub", 0x2D},
    {"F1", 0x3A}, // f1
    {"F2", 0x3B}, // f2
    {"F3", 0x3C}, // f3
    {"F4", 0x3D}, // f4
    {"F5", 0x3E}, // f5
    {"F6", 0x3F}, // f6
    {"F7", 0x40}, // f7
    {"F8", 0x41}, // f8
    {"F9", 0x42}, // f9
    {"F10", 0x43}, // f10
    {"F11", 0x44}, // f11
    {"F12", 0x45}, // f12
    {"!", 0x1E}, // !
    {"#", 0x32}, // #
    {"+", 0x2E}, // +
    {"^", 0x23}, // ^
    {"{", 0x2F}, // {
    {"}", 0x30}, // }
    {"Enter", 0x28}, // Enter
    {"Esc", 0x29}, // esc
    {"Escape", 0x29}, // Escape
    {"Space", 0x2C}, // space
    {"Tab", 0x2B} , // Tab
    {"Backspace", 0x2A}, // Backspace
    {"BS", 0x2A},  //Backspace
    {"Del", 0x4C}, // Del
    {"Delete", 0x4C}, // Delete
    {"Insert", 0x49}, // Insert
    {"Ins", 0x49}, // Ins
    {"Up", 0x52},    // Up
    {"Down", 0x51},  // Down
    {"Left", 0x50}, // Left
    {"Right", 0x4F}, // Right
    {"Home", 0x4A}, // Home
    {"End", 0x4D}, // End
    {"PgUp", 0x4B}, // PgUp
    {"PgDn", 0x4E}, // PgDn
    {"CapsLock", 0x39}, // CapsLock
    {"ScrollLock", 0x47}, // ScrollLock
    {"NumLock", 0x53}, // NumLock
    {"Control", 0xE4}, // Control right
    {"Ctrl", 0xE4}, // Control right
    {"LControl", 0xE0}, // Control left
    {"LCtrl", 0xE0}, // Control left
    {"Alt", 0xE6}, // Alt
    {"RAlt", 0xE6}, // RAlt
    {"Shift", 0xE5}, // Shift
    {"LWin", 0xE3}, // LWin
    {"RWin", 0xE7}, // RWin 
    {"AppsKey", 0x65} // APPWin

};




#endif // KEYBOARDMOUSE_H

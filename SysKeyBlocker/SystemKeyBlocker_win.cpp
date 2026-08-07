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
* ========================================================================== *
*/

#include "SystemKeyBlocker.h"

#include <windows.h>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QAbstractEventDispatcher>

Q_LOGGING_CATEGORY(log_syskey_win, "opf.systemkey.win")

/* ============================================================================
 *  Static self-pointer
 *
 *  WH_KEYBOARD_LL hook proc is a C-style callback — no `this`.  We store
 *  the current active instance so the hook can reach it.
 * ============================================================================ */
SystemKeyBlocker *SystemKeyBlocker::s_self = nullptr;

/* ============================================================================
 *  Modifier key state tracking
 *
 *  Since we swallow key events, GetAsyncKeyState won't work correctly for
 *  modifier keys that we've intercepted. We maintain our own state.
 * ============================================================================ */
struct ModifierKeyState {
    bool lShift = false;
    bool rShift = false;
    bool lCtrl = false;
    bool rCtrl = false;
    bool lAlt = false;
    bool rAlt = false;
    bool lWin = false;
    bool rWin = false;
};
static ModifierKeyState g_modifierState;

/* ============================================================================
 *  startImpl / stopImpl
 * ============================================================================ */

bool SystemKeyBlocker::startImpl(quintptr nativeParentHwnd)
{
    Q_ASSERT(!s_self || s_self == this);
    s_self = this;
    m_hookedHwnd = nativeParentHwnd;

    HINSTANCE hInst = GetModuleHandle(nullptr);
    HHOOK hook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        lowLevelKeyboardProc,
        hInst,
        0                    // hMod = 0 and dwThreadId = 0 → global hook
    );

    if (!hook) {
        DWORD err = GetLastError();
        qCCritical(log_syskey_win)
            << "SetWindowsHookEx(WH_KEYBOARD_LL) failed, error =" << err;
        s_self = nullptr;
        return false;
    }

    m_hHook = hook;
    qCInfo(log_syskey_win) << "Low-level keyboard hook installed (HWND:"
                           << nativeParentHwnd << ")";
    return true;
}

void SystemKeyBlocker::stopImpl()
{
    if (m_hHook) {
        UnhookWindowsHookEx(static_cast<HHOOK>(m_hHook));
        m_hHook = nullptr;
        qCInfo(log_syskey_win) << "Low-level keyboard hook removed";
    }
    s_self = nullptr;
}

/* ============================================================================
 *  Low-level keyboard hook callback
 *
 *  IMPORTANT — runs on a **system thread**, not the Qt event thread.
 *  - Must return within ~10 ms or the system will unhook us.
 *  - We do the absolute minimum here: classify the event, emit the signal
 *    (Qt marshals it across threads), and return.
 *  - Key-down events are swallowed (return 1).  Key-up events are passed
 *    through to avoid stuck keys in the OS.  Same strategy as VirtualBox.
 * ============================================================================ */

LRESULT CALLBACK SystemKeyBlocker::lowLevelKeyboardProc(
    int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode != HC_ACTION || !s_self || !s_self->m_active) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    // Check if openterfaceQT window (or any of its child windows) is in foreground
    // Only intercept keyboard events when our window is active
    HWND foregroundWnd = GetForegroundWindow();
    HWND hookedWnd = (HWND)s_self->m_hookedHwnd;

    // Only block if our window or a child of our window is in foreground
    // Check both directions: foreground is hookedWnd, or foreground is a child of hookedWnd
    // (VideoPane is a child of MainWindow, so when MainWindow is foreground, we need
    // to check if the foreground has VideoPane as a descendant)
    bool isOurWindowFocused = false;
    if (foregroundWnd != nullptr && hookedWnd != nullptr) {
        if (foregroundWnd == hookedWnd) {
            isOurWindowFocused = true;
        } else if (IsChild(hookedWnd, foregroundWnd)) {
            // foregroundWnd is a child of hookedWnd (e.g. VideoPane has a child widget focused)
            isOurWindowFocused = true;
        } else if (IsChild(foregroundWnd, hookedWnd)) {
            // hookedWnd (VideoPane) is a child of foregroundWnd (MainWindow)
            isOurWindowFocused = true;
        }
    }

    if (!isOurWindowFocused) {
        // Our window is not in foreground, let the event pass through
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    const auto *kb = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
    const bool isKeyDown   = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    const bool isExtended  = (kb->flags & LLKHF_EXTENDED) != 0;
    const bool wasInjected = (kb->flags & LLKHF_INJECTED)  != 0;
    const quint32 vk       = kb->vkCode;

    // Let injected events pass through — they were generated by another
    // application (e.g. on-screen keyboard, automation tool), not by
    // physical keystrokes the user intends for the target.
    if (wasInjected) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    // ---- Update modifier state tracking ----
    switch (vk) {
        case VK_LSHIFT:    g_modifierState.lShift = isKeyDown; break;
        case VK_RSHIFT:    g_modifierState.rShift = isKeyDown; break;
        case VK_LCONTROL:  g_modifierState.lCtrl = isKeyDown;  break;
        case VK_RCONTROL:  g_modifierState.rCtrl = isKeyDown;  break;
        case VK_LMENU:     g_modifierState.lAlt = isKeyDown;   break;
        case VK_RMENU:     g_modifierState.rAlt = isKeyDown;   break;
        case VK_LWIN:      g_modifierState.lWin = isKeyDown;   break;
        case VK_RWIN:      g_modifierState.rWin = isKeyDown;   break;
    }

    // ---- Detect "stuck" Win/GUI keys ----
    //
    // Windows does NOT deliver WM_KEYUP for VK_LWIN/VK_RWIN to WH_KEYBOARD_LL
    // hooks when the Start menu (or another system hotkey handler) consumes the
    // key. This leaves our tracked modifier state permanently in the "down"
    // position, and every subsequent keyCaptured carries Qt::MetaModifier —
    // which in turn makes KeyboardManager include LGUI/RGUI in every HID report
    // sent to the target machine, breaking all target-side shortcuts.
    //
    // Fix: on every non-Win event, poll the real physical state via
    // GetAsyncKeyState.  If we believe the Win key is down but the OS reports
    // it as physically released, synthesize a KEYUP so both our state and the
    // target machine recover.  Same idea as VirtualBox's UIKeyboardHandler.
    if (vk != VK_LWIN && vk != VK_RWIN) {
        if (g_modifierState.lWin && !(GetAsyncKeyState(VK_LWIN) & 0x8000)) {
            // Build a modifier mask that still includes Meta, so the target
            // sees this release in the correct modifier context.
            int stuckMods = Qt::MetaModifier;
            if (g_modifierState.lShift || g_modifierState.rShift) stuckMods |= Qt::ShiftModifier;
            if (g_modifierState.lCtrl  || g_modifierState.rCtrl)  stuckMods |= Qt::ControlModifier;
            if (g_modifierState.lAlt   || g_modifierState.rAlt)   stuckMods |= Qt::AltModifier;
            emit s_self->keyCaptured(Qt::Key_Meta, stuckMods, false, VK_LWIN);
            g_modifierState.lWin = false;
            qCInfo(log_syskey_win) << "Synthesized VK_LWIN KEYUP — hook missed the real release "
                                      "(typically caused by Start menu opening on the local OS)";
        }
        if (g_modifierState.rWin && !(GetAsyncKeyState(VK_RWIN) & 0x8000)) {
            int stuckMods = Qt::MetaModifier;
            if (g_modifierState.lShift || g_modifierState.rShift) stuckMods |= Qt::ShiftModifier;
            if (g_modifierState.lCtrl  || g_modifierState.rCtrl)  stuckMods |= Qt::ControlModifier;
            if (g_modifierState.lAlt   || g_modifierState.rAlt)   stuckMods |= Qt::AltModifier;
            emit s_self->keyCaptured(Qt::Key_Meta, stuckMods, false, VK_RWIN);
            g_modifierState.rWin = false;
            qCInfo(log_syskey_win) << "Synthesized VK_RWIN KEYUP — hook missed the real release";
        }
    }

    // ---- Build modifier mask from our tracked state ----
    int modifiers = 0;
    if (g_modifierState.lShift || g_modifierState.rShift) modifiers |= Qt::ShiftModifier;
    if (g_modifierState.lCtrl || g_modifierState.rCtrl)   modifiers |= Qt::ControlModifier;
    if (g_modifierState.lAlt || g_modifierState.rAlt)     modifiers |= Qt::AltModifier;
    if (g_modifierState.lWin || g_modifierState.rWin)     modifiers |= Qt::MetaModifier;

    // ---- Translate to Qt key code ----
    const int qtKey = s_self->nativeToQtKey(vk, isExtended);

    // ---- Emit signal (cross-thread dispatch by Qt) ----
    emit s_self->keyCaptured(qtKey, modifiers, isKeyDown, vk);

    // ---- Conditional swallow based on m_swallowEnabled ----
    // When swallowEnabled (Blocker ON): swallow ALL key events (both down and up).
    //   The local OS won't see any keystrokes — everything goes to the target.
    // When !swallowEnabled (Blocker OFF): pass through to local OS.
    //   Keys are STILL forwarded to the target via the keyCaptured signal above,
    //   but the local OS also processes them normally.
    if (s_self->m_swallowEnabled) {
        return 1;  // swallow — local OS won't see the key
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);  // pass through
}


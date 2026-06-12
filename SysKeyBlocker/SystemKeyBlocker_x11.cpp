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

#include <QLoggingCategory>
#include <QAbstractEventDispatcher>
#include <QGuiApplication>
#include <QtPlatformHeaders/QXcbScreenFunctions>

#include <xcb/xcb.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

Q_LOGGING_CATEGORY(log_syskey_x11, "opf.systemkey.x11")

/* ============================================================================
 *  X11KeyCaptureFilter — inner class
 *
 *  Registered as a QAbstractNativeEventFilter.  XCB key events that arrive
 *  at the Qt event loop are intercepted here *before* any Qt widget sees
 *  them.  We eat key-press events so the OS compositor / window manager
 *  never processes them; key-release events are passed through so the
 *  modifier state tracker inside X stays correct.
 * ============================================================================ */

class SystemKeyBlocker::X11KeyCaptureFilter : public QAbstractNativeEventFilter
{
public:
    X11KeyCaptureFilter(SystemKeyBlocker *b) : blocker(b) {}

    bool nativeEventFilter(const QByteArray &eventType,
                           void *message,
                           long * /*result*/) override
    {
        // We only care about XCB generic events
        if (eventType != QByteArrayLiteral("xcb_generic_event_t"))
            return false;

        if (!blocker || !blocker->isActive())
            return false;

        const auto *ev = static_cast<xcb_generic_event_t *>(message);
        const uint8_t type = ev->response_type & ~0x80;

        if (type != XCB_KEY_PRESS && type != XCB_KEY_RELEASE)
            return false;

        const auto *ke = static_cast<xcb_key_press_event_t *>(message);
        const bool isDown = (type == XCB_KEY_PRESS);

        // ---- Resolve keysym ----
        // X11 connection is already opened by the QPA; get the Display ptr
        // from the current platform native interface.
        Display *dpy = nullptr;
        if (auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
            dpy = reinterpret_cast<Display *>(
                app->platformNativeInterface()->nativeResourceForScreen("display", nullptr));
        }

        if (!dpy) {
            qCWarning(log_syskey_x11) << "Cannot get X11 Display — event dropped";
            return false;
        }

        KeySym ks = XkbKeycodeToKeysym(dpy, ke->detail, 0, 0);

        // ---- Modifiers ----
        int modifiers = 0;
        if (ke->state & XCB_MOD_MASK_SHIFT)   modifiers |= Qt::ShiftModifier;
        if (ke->state & XCB_MOD_MASK_CONTROL) modifiers |= Qt::ControlModifier;
        if (ke->state & XCB_MOD_MASK_1)       modifiers |= Qt::AltModifier;
        if (ke->state & XCB_MOD_MASK_4)       modifiers |= Qt::MetaModifier;

        // ---- Emit ----
        const int qtKey = blocker->nativeToQtKey(static_cast<quint32>(ks), false);
        emit blocker->keyCaptured(qtKey, modifiers, isDown,
                                  static_cast<quint32>(ks));

        // ---- Swallow press, pass release ----
        if (isDown)
            return true;          // intercepted — OS never sees this

        return false;             // release → let X11 update its state
    }

private:
    SystemKeyBlocker *blocker = nullptr;
};

/* ============================================================================
 *  startImpl / stopImpl
 * ============================================================================ */

bool SystemKeyBlocker::startImpl(quintptr /*nativeParentHwnd*/)
{
    qCInfo(log_syskey_x11) << "Installing X11 key capture filter";

    m_x11Filter = new X11KeyCaptureFilter(this);
    QCoreApplication::instance()->installNativeEventFilter(m_x11Filter);
    return true;
}

void SystemKeyBlocker::stopImpl()
{
    if (m_x11Filter) {
        qCInfo(log_syskey_x11) << "Removing X11 key capture filter";
        QCoreApplication::instance()->removeNativeEventFilter(m_x11Filter);
        delete m_x11Filter;
        m_x11Filter = nullptr;
    }
}


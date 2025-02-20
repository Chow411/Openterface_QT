#include "KeyboardFilter.h"
#include "serial/SerialPortManager.h"
#include "host/HostManager.h"
KeyboardFilter::KeyboardFilter(QObject *parent)
                            : QObject(parent)
                            , m_keyHook(Q_NULLPTR)
{
}

KeyboardFilter::~KeyboardFilter(){
    uninstallKeyHook();
}


bool KeyboardFilter::uninstallKeyHook()
{
    if(Q_UNLIKELY(!m_keyHook)){
        return true;
    }

    bool status = false;
    status = UnhookWindowsHookEx(m_keyHook);
    if(status) m_keyHook = Q_NULLPTR;

    return status;
}

bool KeyboardFilter::installKeyHook()
{
    m_keyHook = SetWindowsHookEx(WH_KEYBOARD_LL,keyboardHookProc, (HINSTANCE)GetModuleHandle(nullptr), 0);
    return Q_UNLIKELY(m_keyHook);
}

bool KeyboardFilter::isKeyHookRunning()
{
    return Q_UNLIKELY(m_keyHook);
}

LRESULT CALLBACK KeyboardFilter::keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0){
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            if (GetAsyncKeyState(VK_MENU) && p->vkCode == VK_TAB){
                emit SerialPortManager::getInstance().sendCommandAsync(CMD_SEND_KB_GENERAL_DATA, false);
                QThread::usleep(1000);
                qDebug() << "alt && tab press down >>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<";
            }
        }
        if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP){
            // if ()
            // qDebug() << "alt up";
            if(GetAsyncKeyState(VK_MENU)){
                qDebug() << "alt up >>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<";
                QKeyEvent *event = new QKeyEvent(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
                
                HostManager::getInstance().handleKeyRelease(event);
                
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

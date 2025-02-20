#ifndef KEYBOARDFILTER_H
#define KEYBOARDFILTER_H

#ifdef _WIN32
#include <windows.h>
#endif
#include <QObject>
#include <QDebug>
#include <QKeyEvent>

class KeyboardFilter : public QObject
{
    Q_OBJECT

public:
    explicit KeyboardFilter(QObject *parent = nullptr);
    ~KeyboardFilter();
    bool installKeyHook();
    bool uninstallKeyHook();
    bool isKeyHookRunning();


private:
    HHOOK m_keyHook = Q_NULLPTR;
    static LRESULT CALLBACK keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
};


#endif
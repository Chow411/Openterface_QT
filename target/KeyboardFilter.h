#ifndef KEYBOARDFILTER_H
#define KEYBOARDFILTER_H

#ifdef _WIN32
#include <windows.h>
#endif
#include <QObject>

class KeyboardFilter : public QObject
{
    Q_OBJECT

public:
    explicit KeyboardFilter(QObject *parent = nullptr);
    ~KeyboardFilter();

private:
    HHOOK m_hHook;
};


#endif
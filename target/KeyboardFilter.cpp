#include "KeyboardFilter.h"

KeyboardFilter::KeyboardFilter(QObject *parent)
                            : QObject(parent)
                            , m_hHook(NULL)
{

}

KeyboardFilter::~KeyboardFilter(){

}

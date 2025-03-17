#include "languagemanager.h"
#include "globalsetting.h"

#include <QDir>
#include <QDebug>

LanguageManager::LanguageManager(QApplication *app, QObject *parent)
    : QObject(parent),
      m_app(app),
      m_translator(new QTranslator(this)),
      m_translationPath("config/languages/") {

}

LanguageManager::~LanguageManager() {
    delete m_translator;
}

void LanguageManager::initialize(const QString &defaultLanguage) {
    GlobalSetting::instance().getLanguage(m_currentLanguage); 
    switchLanguage(m_currentLanguage);
}

void LanguageManager::switchLanguage(const QString &language) {
    if (!m_currentLanguage.isEmpty()) {
        m_app->removeTranslator(m_translator);
    }

    QString filePath = m_translationPath + "openterface_" + language + ".qm";
    if (m_translator->load(filePath)) {
        m_app->installTranslator(m_translator);
        m_currentLanguage = language;
        GlobalSetting::instance().setLangeuage(m_currentLanguage);
        emit languageChanged();
    } else {
        qWarning() << "Failed to load translation file:" << filePath;
    }
}

QStringList LanguageManager::availableLanguages() const {
    QDir dir(m_translationPath);
    QStringList filters;
    filters << "openterface_*.qm";
    QStringList files = dir.entryList(filters, QDir::Files);
    
    QStringList languages;
    for (const QString &file : files) {
        QString lang = file.mid(strlen("openterface_"), 2);
        languages << lang;
    }
    return languages;
}
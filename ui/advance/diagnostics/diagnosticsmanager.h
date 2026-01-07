#ifndef DIAGNOSTICSMANAGER_H
#define DIAGNOSTICSMANAGER_H

#include <QObject>
#include <QTimer>
#include <QStringList>
#include <QVector>
#include "diagnosticstypes.h" // for TestStatus

class DiagnosticsManager : public QObject
{
    Q_OBJECT
public:
    explicit DiagnosticsManager(QObject *parent = nullptr);

    QStringList testTitles() const { return m_testTitles; }
    TestStatus testStatus(int index) const;
    QString getTestTitle(int index) const;
    QString getLogFilePath() const;
    bool isTestingInProgress() const { return m_isTestingInProgress; }

public slots:
    void startTest(int index);
    void resetAllTests();

signals:
    void testStarted(int index);
    void testCompleted(int index, bool success);
    void diagnosticsCompleted(bool allSuccessful);
    void logAppended(const QString &entry);
    void statusChanged(int index, TestStatus status);

private slots:
    void onTimerTimeout();

private:
    void appendToLog(const QString &message);

    QStringList m_testTitles;
    QVector<TestStatus> m_statuses;
    QTimer *m_testTimer;
    int m_runningTestIndex;
    bool m_isTestingInProgress;
};

#endif // DIAGNOSTICSMANAGER_H

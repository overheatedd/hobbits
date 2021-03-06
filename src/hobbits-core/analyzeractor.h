#ifndef ANALYZERACTOR_H
#define ANALYZERACTOR_H

#include "actionwatcher.h"
#include "analyzerresult.h"
#include "hobbitspluginmanager.h"
#include <QJsonObject>
#include <QObject>
#include <QtConcurrent/QtConcurrentRun>

#include "hobbits-core_global.h"

class PluginActionManager;

class HOBBITSCORESHARED_EXPORT AnalyzerActor : public QObject
{
    Q_OBJECT

public:
    explicit AnalyzerActor(PluginActionManager *manager, QSharedPointer<const HobbitsPluginManager> pluginManager);

    QSharedPointer<ActionWatcher<QSharedPointer<const AnalyzerResult>>> analyzerFullAct(
            QSharedPointer<AnalyzerInterface> analyzer,
            QSharedPointer<BitContainer> container,
            QJsonObject pluginState = QJsonObject());

signals:
    void reportError(QString);

public slots:
private slots:
    void postProcess();

private:
    static QSharedPointer<const AnalyzerResult> analyzerCall(
            QSharedPointer<AnalyzerInterface> analyzer,
            QSharedPointer<const BitContainer> bits,
            QJsonObject pluginState,
            QSharedPointer<ActionProgress> progressTracker);

    QFuture<QSharedPointer<const AnalyzerResult>> m_future;
    QSharedPointer<AnalyzerInterface> m_analyzer;
    QSharedPointer<BitContainer> m_container;
    QJsonObject m_pluginState;
    QSharedPointer<const HobbitsPluginManager> m_pluginManager;

    QSharedPointer<ActionWatcher<QSharedPointer<const AnalyzerResult>>> m_actionWatcher;

    PluginActionManager *m_manager;
};

#endif // ANALYZERACTOR_H

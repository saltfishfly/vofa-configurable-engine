#ifndef CONFIGURABLEENGINE_H
#define CONFIGURABLEENGINE_H

#include "dataengineinterface.h"
#include "frameparser.h"

#include <QDateTime>
#include <QObject>
#include <QString>

class ConfigurableEngine : public QObject, public DataEngineInterface
{
    Q_OBJECT
    Q_INTERFACES(DataEngineInterface)
    Q_PLUGIN_METADATA(IID "VOFA+.Plugin.ConfigurableEngine")

public:
    explicit ConfigurableEngine();
    ~ConfigurableEngine() override;
    void ProcessingDatas(char *data, int count) override;

private:
    QString locateConfig() const;
    bool reloadConfig(bool force);

    ProtocolConfig config_;
    FrameParser parser_;
    QString configPath_;
    QDateTime configModified_;
    qint64 lastConfigCheckMs_ = 0;
    bool configValid_ = false;
};

#endif

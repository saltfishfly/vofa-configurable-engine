#include "configurableengine.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

ConfigurableEngine::ConfigurableEngine()
{
    configPath_ = locateConfig();
    reloadConfig(true);
}

ConfigurableEngine::~ConfigurableEngine()
{
    qDeleteAll(image_channels_);
    image_channels_.clear();
}

QString ConfigurableEngine::locateConfig() const
{
    const QByteArray overridePath = qgetenv("VOFA_PROTOCOL_CONFIG");
    if (!overridePath.isEmpty()) return QFileInfo(QString::fromLocal8Bit(overridePath)).absoluteFilePath();

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath("plugins/dataengines/configurable_engine.json"),
        QDir(appDir).filePath("configurable_engine.json"),
        QDir::current().filePath("configurable_engine.json")
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) return QFileInfo(candidate).absoluteFilePath();
    }
    return QFileInfo(candidates.first()).absoluteFilePath();
}

bool ConfigurableEngine::reloadConfig(bool force)
{
    const QFileInfo info(configPath_);
    if (!force && info.exists() && info.lastModified() == configModified_) return configValid_;

    ProtocolConfig next;
    QString error;
    if (!ProtocolConfig::loadFile(configPath_, &next, &error)) {
        configValid_ = false;
        qWarning().noquote() << "ConfigurableEngine:" << error;
        return false;
    }

    config_ = next;
    parser_.setConfig(config_);
    configModified_ = info.lastModified();
    configValid_ = true;
    qInfo().noquote() << "ConfigurableEngine: loaded" << config_.name << "from" << configPath_;
    return true;
}

void ConfigurableEngine::ProcessingDatas(char *data, int count)
{
    frame_list_.clear();
    if (!data || count <= 0) return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastConfigCheckMs_ >= 1000) {
        lastConfigCheckMs_ = now;
        reloadConfig(false);
    }

    if (!configValid_) {
        Frame invalid;
        invalid.start_index_ = 0;
        invalid.end_index_ = count - 1;
        invalid.is_valid_ = false;
        frame_list_.append(invalid);
        return;
    }

    const QVector<ParsedFrame> parsed = parser_.parse(QByteArray(data, count));
    for (const ParsedFrame &item : parsed) {
        Frame frame;
        frame.start_index_ = item.start;
        frame.end_index_ = item.end;
        frame.image_size_ = 0;
        frame.datas_ = item.values;
        frame.is_valid_ = item.valid;
        frame_list_.append(frame);
        if (!item.valid && !item.error.isEmpty())
            qDebug().noquote() << "ConfigurableEngine:" << item.error
                               << "at" << item.start << ".." << item.end;
    }
}

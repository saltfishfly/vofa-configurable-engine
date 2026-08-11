#include "dataengineinterface.h"

#include <QCoreApplication>
#include <QDebug>
#include <QPluginLoader>
#include <cmath>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 3) qFatal("usage: plugin_smoke <plugin.dll> <config.json>");
    qputenv("VOFA_PROTOCOL_CONFIG", QByteArray(argv[2]));

    QPluginLoader loader(QString::fromLocal8Bit(argv[1]));
    QObject *object = loader.instance();
    if (!object) qFatal("plugin load failed: %s", qPrintable(loader.errorString()));

    DataEngineInterface *engine = qobject_cast<DataEngineInterface *>(object);
    if (!engine) qFatal("qobject_cast<DataEngineInterface*> failed");

    QByteArray frame = QByteArray::fromHex("AA552EFB CD8B0100 00005040 0D0A");
    engine->ProcessingDatas(frame.data(), frame.size());
    const QList<Frame> &frames = engine->frame_list();
    if (frames.size() != 1 || !frames[0].is_valid_ || frames[0].datas_.size() != 3)
        qFatal("plugin returned an unexpected frame");
    if (std::fabs(frames[0].datas_[0] + 12.34f) > 0.001f ||
        frames[0].datas_[1] != 101325.0f ||
        std::fabs(frames[0].datas_[2] - 3.25f) > 0.001f)
        qFatal("plugin decoded unexpected sample values");

    qInfo() << "Plugin smoke test passed.";
    loader.unload();
    return 0;
}

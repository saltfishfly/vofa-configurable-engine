#ifndef FRAMEPARSER_H
#define FRAMEPARSER_H

#include "protocolconfig.h"

#include <QByteArray>
#include <QString>
#include <QVector>

struct ParsedFrame
{
    int start = 0;
    int end = 0;
    bool valid = false;
    QVector<float> values;
    QString error;
};

class FrameParser
{
public:
    explicit FrameParser(const ProtocolConfig &config = ProtocolConfig());
    void setConfig(const ProtocolConfig &config);
    QVector<ParsedFrame> parse(const QByteArray &buffer) const;

private:
    ProtocolConfig config_;
};

#endif

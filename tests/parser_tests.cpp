#include "frameparser.h"
#include "protocolconfig.h"

#include <QCoreApplication>
#include <QDebug>
#include <QtGlobal>
#include <cmath>
#include <cstring>

namespace {

void require(bool condition, const char *message)
{
    if (!condition) qFatal("TEST FAILED: %s", message);
}

QByteArray fixedFrame(qint16 temperature, quint32 pressure, float voltage)
{
    QByteArray frame = QByteArray::fromHex("AA55");
    frame.append(static_cast<char>(temperature & 0xff));
    frame.append(static_cast<char>((temperature >> 8) & 0xff));
    for (int i = 0; i < 4; ++i) frame.append(static_cast<char>((pressure >> (8 * i)) & 0xff));
    quint32 voltageBits = 0;
    std::memcpy(&voltageBits, &voltage, sizeof(voltage));
    for (int i = 0; i < 4; ++i) frame.append(static_cast<char>((voltageBits >> (8 * i)) & 0xff));
    frame.append(QByteArray::fromHex("0D0A"));
    return frame;
}

void testFixedFrame()
{
    const QByteArray json = R"JSON({
      "name":"test",
      "framing":{"header":"AA55","tail":"0D0A","fixed_length":14,"minimum_length":14,"maximum_length":14},
      "fields":[
        {"name":"t","offset":2,"type":"int16","endian":"little","scale":0.01},
        {"name":"p","offset":4,"type":"uint32","endian":"little"},
        {"name":"v","offset":8,"type":"float32","endian":"little"}
      ]
    })JSON";
    ProtocolConfig config;
    QString error;
    require(ProtocolConfig::fromJson(json, &config, &error), qPrintable(error));
    FrameParser parser(config);

    QByteArray input = QByteArray::fromHex("0099");
    input += fixedFrame(-1234, 101325, 3.25f);
    input += fixedFrame(2500, 90000, 1.5f).left(7);
    const QVector<ParsedFrame> frames = parser.parse(input);
    require(frames.size() == 2, "noise + one complete frame expected");
    require(!frames[0].valid && frames[0].start == 0 && frames[0].end == 1, "noise range");
    require(frames[1].valid && frames[1].values.size() == 3, "valid sample frame");
    require(std::fabs(frames[1].values[0] + 12.34f) < 0.001f, "signed/scaled int16");
    require(frames[1].values[1] == 101325.0f, "uint32");
    require(std::fabs(frames[1].values[2] - 3.25f) < 0.001f, "float32");
}

void testTailFramingAndChecksum()
{
    const QByteArray json = R"JSON({
      "name":"tail",
      "framing":{"header":"7E","tail":"0A","minimum_length":4,"maximum_length":32},
      "fields":[{"name":"x","offset":1,"type":"uint8"}],
      "checksum":{"type":"sum8","offset":2,"range_start":0,"range_end":2}
    })JSON";
    ProtocolConfig config;
    QString error;
    require(ProtocolConfig::fromJson(json, &config, &error), qPrintable(error));
    FrameParser parser(config);
    const QByteArray input = QByteArray::fromHex("7E05 83 0A 7E06 00 0A");
    const QVector<ParsedFrame> frames = parser.parse(input);
    require(frames.size() == 2, "two tail-delimited frames");
    require(frames[0].valid && frames[0].values[0] == 5.0f, "sum8 pass");
    require(!frames[1].valid, "sum8 reject");
}

void testLengthField()
{
    const QByteArray json = R"JSON({
      "name":"length",
      "framing":{"header":"A5","minimum_length":4,"maximum_length":32,
                  "length_field":{"offset":1,"size":1,"endian":"little","adjust":2}},
      "fields":[{"name":"x","offset":2,"type":"uint16","endian":"big"}]
    })JSON";
    ProtocolConfig config;
    QString error;
    require(ProtocolConfig::fromJson(json, &config, &error), qPrintable(error));
    FrameParser parser(config);
    const QVector<ParsedFrame> frames = parser.parse(QByteArray::fromHex("A5021234"));
    require(frames.size() == 1 && frames[0].valid, "length field frame");
    require(frames[0].values[0] == 0x1234, "big endian uint16");
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testFixedFrame();
    testTailFramingAndChecksum();
    testLengthField();
    qInfo() << "All parser tests passed.";
    return 0;
}

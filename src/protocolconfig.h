#ifndef PROTOCOLCONFIG_H
#define PROTOCOLCONFIG_H

#include <QByteArray>
#include <QString>
#include <QVector>

enum class ByteOrder { LittleEndian, BigEndian };

struct FieldSpec
{
    QString name;
    int offset = 0;
    QString type;
    ByteOrder byteOrder = ByteOrder::LittleEndian;
    double scale = 1.0;
    double bias = 0.0;
};

struct LengthFieldSpec
{
    bool enabled = false;
    int offset = 0;
    int size = 1;
    ByteOrder byteOrder = ByteOrder::LittleEndian;
    int adjust = 0;
};

struct ChecksumSpec
{
    QString type = "none";
    int offset = 0;
    int rangeStart = 0;
    int rangeEnd = -1;
    ByteOrder byteOrder = ByteOrder::LittleEndian;
};

struct ProtocolConfig
{
    QString name = "ConfigurableEngine";
    QByteArray header;
    QByteArray tail;
    int fixedLength = 0;
    int minimumLength = 1;
    int maximumLength = 65536;
    LengthFieldSpec lengthField;
    ChecksumSpec checksum;
    QVector<FieldSpec> fields;

    static bool loadFile(const QString &path, ProtocolConfig *config, QString *error);
    static bool fromJson(const QByteArray &json, ProtocolConfig *config, QString *error);
    bool validate(QString *error) const;
};

#endif

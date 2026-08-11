#include "frameparser.h"

#include <QtEndian>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

quint64 readUnsigned(const char *data, int size, ByteOrder order)
{
    quint64 value = 0;
    if (order == ByteOrder::LittleEndian) {
        for (int i = size - 1; i >= 0; --i)
            value = (value << 8) | static_cast<quint8>(data[i]);
    } else {
        for (int i = 0; i < size; ++i)
            value = (value << 8) | static_cast<quint8>(data[i]);
    }
    return value;
}

qint64 signExtend(quint64 value, int bits)
{
    if (bits >= 64) return static_cast<qint64>(value);
    const quint64 sign = quint64(1) << (bits - 1);
    if (value & sign) value |= (~quint64(0) << bits);
    return static_cast<qint64>(value);
}

int fieldSize(const QString &type)
{
    const QString t = type.toLower();
    if (t == "int8" || t == "uint8") return 1;
    if (t == "int16" || t == "uint16") return 2;
    if (t == "int32" || t == "uint32" || t == "float32" || t == "float") return 4;
    if (t == "int64" || t == "uint64" || t == "float64" || t == "double") return 8;
    return 0;
}

bool decodeField(const QByteArray &frame, const FieldSpec &field, double *result)
{
    const int size = fieldSize(field.type);
    if (size == 0 || field.offset < 0 || field.offset + size > frame.size()) return false;

    const char *source = frame.constData() + field.offset;
    const quint64 raw = readUnsigned(source, size, field.byteOrder);
    const QString type = field.type.toLower();
    double value = 0.0;

    if (type.startsWith("uint")) {
        value = static_cast<double>(raw);
    } else if (type.startsWith("int")) {
        value = static_cast<double>(signExtend(raw, size * 8));
    } else if (type == "float32" || type == "float") {
        const quint32 bits = static_cast<quint32>(raw);
        float decoded = 0.0f;
        std::memcpy(&decoded, &bits, sizeof(decoded));
        value = decoded;
    } else if (type == "float64" || type == "double") {
        const quint64 bits = raw;
        double decoded = 0.0;
        std::memcpy(&decoded, &bits, sizeof(decoded));
        value = decoded;
    } else {
        return false;
    }

    value = value * field.scale + field.bias;
    if (!std::isfinite(value) || value > std::numeric_limits<float>::max() ||
        value < -std::numeric_limits<float>::max()) {
        return false;
    }
    *result = value;
    return true;
}

quint16 crc16Modbus(const char *data, int size)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < size; ++i) {
        crc ^= static_cast<quint8>(data[i]);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 1) ? static_cast<quint16>((crc >> 1) ^ 0xA001) : (crc >> 1);
    }
    return crc;
}

bool validateChecksum(const QByteArray &frame, const ChecksumSpec &spec, QString *error)
{
    if (spec.type == "none") return true;
    const int checksumSize = spec.type == "crc16_modbus" ? 2 : 1;
    const int rangeEnd = spec.rangeEnd < 0 ? spec.offset : spec.rangeEnd;
    if (spec.offset < 0 || spec.offset + checksumSize > frame.size() ||
        spec.rangeStart < 0 || rangeEnd < spec.rangeStart || rangeEnd > frame.size()) {
        if (error) *error = "校验字段或校验范围越界";
        return false;
    }

    quint64 calculated = 0;
    if (spec.type == "sum8") {
        for (int i = spec.rangeStart; i < rangeEnd; ++i)
            calculated = (calculated + static_cast<quint8>(frame[i])) & 0xFF;
    } else if (spec.type == "xor8") {
        for (int i = spec.rangeStart; i < rangeEnd; ++i)
            calculated ^= static_cast<quint8>(frame[i]);
    } else if (spec.type == "crc16_modbus") {
        calculated = crc16Modbus(frame.constData() + spec.rangeStart,
                                 rangeEnd - spec.rangeStart);
    }

    const quint64 expected = readUnsigned(frame.constData() + spec.offset,
                                          checksumSize, spec.byteOrder);
    if (expected != calculated) {
        if (error) *error = QString("校验失败：期望 0x%1，计算得到 0x%2")
                                .arg(expected, 0, 16).arg(calculated, 0, 16);
        return false;
    }
    return true;
}

int suffixPrefixLength(const QByteArray &buffer, int from, const QByteArray &header)
{
    if (header.isEmpty()) return 0;
    const int available = buffer.size() - from;
    const int maximum = qMin(header.size() - 1, available);
    for (int length = maximum; length > 0; --length) {
        if (buffer.mid(buffer.size() - length, length) == header.left(length)) return length;
    }
    return 0;
}

void appendInvalid(QVector<ParsedFrame> *frames, int start, int end, const QString &error)
{
    if (end < start) return;
    ParsedFrame frame;
    frame.start = start;
    frame.end = end;
    frame.valid = false;
    frame.error = error;
    frames->append(frame);
}

} // namespace

FrameParser::FrameParser(const ProtocolConfig &config) : config_(config) {}

void FrameParser::setConfig(const ProtocolConfig &config)
{
    config_ = config;
}

QVector<ParsedFrame> FrameParser::parse(const QByteArray &buffer) const
{
    QVector<ParsedFrame> frames;
    int cursor = 0;

    while (cursor < buffer.size()) {
        int start = cursor;
        if (!config_.header.isEmpty()) {
            start = buffer.indexOf(config_.header, cursor);
            if (start < 0) {
                const int keep = suffixPrefixLength(buffer, cursor, config_.header);
                appendInvalid(&frames, cursor, buffer.size() - keep - 1, "未匹配帧头的噪声");
                break;
            }
            appendInvalid(&frames, cursor, start - 1, "帧头前噪声");
        }

        int frameLength = 0;
        if (config_.fixedLength > 0) {
            frameLength = config_.fixedLength;
        } else if (config_.lengthField.enabled) {
            const int required = config_.lengthField.offset + config_.lengthField.size;
            if (buffer.size() - start < required) break;
            const quint64 encoded = readUnsigned(buffer.constData() + start + config_.lengthField.offset,
                                                 config_.lengthField.size,
                                                 config_.lengthField.byteOrder);
            if (encoded > static_cast<quint64>(std::numeric_limits<int>::max())) {
                appendInvalid(&frames, start, start, "长度字段溢出");
                cursor = start + 1;
                continue;
            }
            frameLength = static_cast<int>(encoded) + config_.lengthField.adjust;
        } else {
            const int tailIndex = buffer.indexOf(config_.tail, start + config_.header.size());
            if (tailIndex < 0) break;
            frameLength = tailIndex + config_.tail.size() - start;
        }

        if (frameLength < config_.minimumLength || frameLength > config_.maximumLength) {
            appendInvalid(&frames, start, start, "帧长度超出配置范围");
            cursor = start + 1;
            continue;
        }
        if (buffer.size() - start < frameLength) break;

        const QByteArray candidate = buffer.mid(start, frameLength);
        if (!config_.tail.isEmpty() && !candidate.endsWith(config_.tail)) {
            appendInvalid(&frames, start, start, "帧尾不匹配");
            cursor = start + 1;
            continue;
        }

        ParsedFrame parsed;
        parsed.start = start;
        parsed.end = start + frameLength - 1;
        parsed.valid = validateChecksum(candidate, config_.checksum, &parsed.error);

        if (parsed.valid) {
            for (const FieldSpec &field : config_.fields) {
                double value = 0.0;
                if (!decodeField(candidate, field, &value)) {
                    parsed.valid = false;
                    parsed.error = QString("字段 %1 越界、类型非法或数值不可表示").arg(field.name);
                    parsed.values.clear();
                    break;
                }
                parsed.values.append(static_cast<float>(value));
            }
        }

        frames.append(parsed);
        cursor = parsed.end + 1;
    }
    return frames;
}

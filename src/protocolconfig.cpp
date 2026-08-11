#include "protocolconfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>

namespace {

bool parseByteOrder(const QString &text, ByteOrder *order)
{
    const QString normalized = text.trimmed().toLower();
    if (normalized == "little" || normalized == "le") {
        *order = ByteOrder::LittleEndian;
        return true;
    }
    if (normalized == "big" || normalized == "be") {
        *order = ByteOrder::BigEndian;
        return true;
    }
    return false;
}

bool parseHexBytes(const QString &text, QByteArray *bytes, QString *error)
{
    QString compact = text;
    compact.remove(QRegularExpression("(?:0x|0X|[\\s,:;_\\-])"));
    if (compact.isEmpty()) {
        bytes->clear();
        return true;
    }
    if ((compact.size() % 2) != 0 ||
        compact.contains(QRegularExpression("[^0-9a-fA-F]"))) {
        if (error) *error = QString("非法十六进制字节串：%1").arg(text);
        return false;
    }
    *bytes = QByteArray::fromHex(compact.toLatin1());
    return true;
}

int typeSize(const QString &type)
{
    static const QHash<QString, int> sizes = {
        {"int8", 1}, {"uint8", 1},
        {"int16", 2}, {"uint16", 2},
        {"int32", 4}, {"uint32", 4}, {"float32", 4}, {"float", 4},
        {"int64", 8}, {"uint64", 8}, {"float64", 8}, {"double", 8}
    };
    return sizes.value(type.toLower(), 0);
}

} // namespace

bool ProtocolConfig::loadFile(const QString &path, ProtocolConfig *config, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QString("无法打开配置文件 %1：%2").arg(path, file.errorString());
        return false;
    }
    return fromJson(file.readAll(), config, error);
}

bool ProtocolConfig::fromJson(const QByteArray &json, ProtocolConfig *config, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QString("JSON 解析失败：%1").arg(parseError.errorString());
        return false;
    }

    ProtocolConfig result;
    const QJsonObject root = document.object();
    result.name = root.value("name").toString(result.name);

    const QJsonObject framing = root.value("framing").toObject();
    if (!parseHexBytes(framing.value("header").toString(), &result.header, error) ||
        !parseHexBytes(framing.value("tail").toString(), &result.tail, error)) {
        return false;
    }
    result.fixedLength = framing.value("fixed_length").toInt(0);
    result.minimumLength = framing.value("minimum_length").toInt(1);
    result.maximumLength = framing.value("maximum_length").toInt(65536);

    if (framing.value("length_field").isObject()) {
        const QJsonObject length = framing.value("length_field").toObject();
        result.lengthField.enabled = true;
        result.lengthField.offset = length.value("offset").toInt(-1);
        result.lengthField.size = length.value("size").toInt(1);
        result.lengthField.adjust = length.value("adjust").toInt(0);
        if (!parseByteOrder(length.value("endian").toString("little"),
                            &result.lengthField.byteOrder)) {
            if (error) *error = "length_field.endian 必须为 little 或 big";
            return false;
        }
    }

    if (root.value("checksum").isObject()) {
        const QJsonObject checksum = root.value("checksum").toObject();
        result.checksum.type = checksum.value("type").toString("none").toLower();
        result.checksum.offset = checksum.value("offset").toInt(-1);
        result.checksum.rangeStart = checksum.value("range_start").toInt(0);
        result.checksum.rangeEnd = checksum.value("range_end").toInt(-1);
        if (!parseByteOrder(checksum.value("endian").toString("little"),
                            &result.checksum.byteOrder)) {
            if (error) *error = "checksum.endian 必须为 little 或 big";
            return false;
        }
    }

    const QJsonArray fields = root.value("fields").toArray();
    for (const QJsonValue &value : fields) {
        if (!value.isObject()) {
            if (error) *error = "fields 中的每一项都必须是对象";
            return false;
        }
        const QJsonObject object = value.toObject();
        if (!object.value("enabled").toBool(true)) continue;

        FieldSpec field;
        field.name = object.value("name").toString();
        field.offset = object.value("offset").toInt(-1);
        field.type = object.value("type").toString().toLower();
        field.scale = object.value("scale").toDouble(1.0);
        field.bias = object.value("bias").toDouble(0.0);
        if (!parseByteOrder(object.value("endian").toString("little"), &field.byteOrder)) {
            if (error) *error = QString("字段 %1 的 endian 必须为 little 或 big").arg(field.name);
            return false;
        }
        result.fields.append(field);
    }

    if (!result.validate(error)) return false;
    *config = result;
    return true;
}

bool ProtocolConfig::validate(QString *error) const
{
    auto fail = [error](const QString &message) {
        if (error) *error = message;
        return false;
    };

    if (name.trimmed().isEmpty()) return fail("name 不能为空");
    if (minimumLength < 1 || maximumLength < minimumLength)
        return fail("minimum_length/maximum_length 范围非法");
    if (fixedLength < 0 || fixedLength > maximumLength)
        return fail("fixed_length 超出允许范围");
    if (fixedLength == 0 && !lengthField.enabled && tail.isEmpty())
        return fail("必须配置 fixed_length、length_field 或 tail 中的至少一种定帧方式");
    if (fixedLength > 0 && fixedLength < minimumLength)
        return fail("fixed_length 小于 minimum_length");
    if (lengthField.enabled) {
        if (lengthField.offset < 0 || !QSet<int>({1, 2, 4}).contains(lengthField.size))
            return fail("length_field 的 offset 或 size 非法（size 仅支持 1/2/4）");
    }

    QSet<QString> names;
    for (const FieldSpec &field : fields) {
        if (field.name.trimmed().isEmpty()) return fail("字段 name 不能为空");
        if (names.contains(field.name)) return fail(QString("字段名重复：%1").arg(field.name));
        names.insert(field.name);
        if (field.offset < 0 || typeSize(field.type) == 0)
            return fail(QString("字段 %1 的 offset 或 type 非法").arg(field.name));
    }

    static const QSet<QString> checksums = {"none", "sum8", "xor8", "crc16_modbus"};
    if (!checksums.contains(checksum.type))
        return fail(QString("不支持的 checksum.type：%1").arg(checksum.type));
    if (checksum.type != "none" && checksum.offset < 0)
        return fail("启用校验时 checksum.offset 不能小于 0");
    if (checksum.rangeStart < 0)
        return fail("checksum.range_start 不能小于 0");
    return true;
}

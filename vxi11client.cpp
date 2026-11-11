#include "vxi11client.h"

#include <algorithm>
#include <array>
#include <limits>

namespace {
constexpr quint32 timeoutAttribute = 0x3FFF001A; // VI_ATTR_TMO_VALUE
constexpr qint32 successMaxCount = 0x3FFF0006;   // VI_SUCCESS_MAX_CNT
constexpr int readChunkSize = 1000;
constexpr int maxResponseSize = 1024 * 1024;
}

Vxi11Client::Vxi11Client(QObject *parent) : QObject(parent) {}

bool Vxi11Client::loadVisa(QString *error)
{
    if (library_.isLoaded())
        return true;

#ifdef Q_OS_WIN
    library_.setFileName(sizeof(void *) == 8 ? QStringLiteral("visa64")
                                             : QStringLiteral("visa32"));
#else
    library_.setFileName(QStringLiteral("visa"));
#endif
    if (!library_.load()) {
        *error = QStringLiteral("无法加载 VISA 库：%1。请安装与程序位数匹配的 NI-VISA Runtime。")
                     .arg(library_.errorString());
        return false;
    }

    openDefaultRM_ = reinterpret_cast<OpenDefaultRM>(library_.resolve("viOpenDefaultRM"));
    open_ = reinterpret_cast<Open>(library_.resolve("viOpen"));
    close_ = reinterpret_cast<Close>(library_.resolve("viClose"));
    setAttribute_ = reinterpret_cast<SetAttribute>(library_.resolve("viSetAttribute"));
    write_ = reinterpret_cast<Write>(library_.resolve("viWrite"));
    read_ = reinterpret_cast<Read>(library_.resolve("viRead"));
    statusDesc_ = reinterpret_cast<StatusDesc>(library_.resolve("viStatusDesc"));
    if (!openDefaultRM_ || !open_ || !close_ || !setAttribute_ || !write_ ||
        !read_ || !statusDesc_) {
        *error = QStringLiteral("VISA 库缺少必要的 vi* 函数：%1").arg(library_.fileName());
        library_.unload();
        return false;
    }
    return true;
}

QString Vxi11Client::errorText(const QString &action, Status status) const
{
    const QString code = QStringLiteral("0x%1").arg(quint32(status), 8, 16, QLatin1Char('0')).toUpper();
    std::array<char, 256> description{};
    const Session session = instrument_ ? instrument_ : resourceManager_;
    if (session && statusDesc_ && statusDesc_(session, status, description.data()) >= 0) {
        const QByteArray bytes(description.data());
        QString decoded = QString::fromUtf8(bytes);
        if (decoded.toUtf8() != bytes)
            decoded = QString::fromLocal8Bit(bytes);
        return QStringLiteral("%1 失败 (%2)：%3").arg(action, code, decoded);
    }
    return QStringLiteral("%1 失败 (%2)").arg(action, code);
}

void Vxi11Client::closeSessions()
{
    if (instrument_ && close_)
        close_(instrument_);
    instrument_ = 0;
    if (resourceManager_ && close_)
        close_(resourceManager_);
    resourceManager_ = 0;
}

void Vxi11Client::connectDevice(const QString &resource, int timeoutMs)
{
    if (instrument_) {
        emit connectionChanged(true, QStringLiteral("已连接，请先断开当前设备。"));
        return;
    }
    QString error;
    if (!loadVisa(&error)) {
        emit connectionChanged(false, error);
        return;
    }
    const QByteArray address = resource.trimmed().toLatin1();
    if (address.isEmpty() || resource.trimmed() != QString::fromLatin1(address)) {
        emit connectionChanged(false, QStringLiteral("资源名不能为空，且必须使用 ASCII 字符。"));
        return;
    }

    Status status = openDefaultRM_(&resourceManager_);
    if (status < 0) {
        error = errorText(QStringLiteral("viOpenDefaultRM"), status);
        closeSessions();
        emit connectionChanged(false, error);
        return;
    }
    status = open_(resourceManager_, address.constData(), 0, UInt32(timeoutMs), &instrument_);
    if (status < 0) {
        error = errorText(QStringLiteral("viOpen"), status);
        closeSessions();
        emit connectionChanged(false, error);
        return;
    }
    status = setAttribute_(instrument_, timeoutAttribute, quintptr(timeoutMs));
    if (status < 0) {
        error = errorText(QStringLiteral("设置 I/O 超时"), status);
        closeSessions();
        emit connectionChanged(false, error);
        return;
    }
    emit connectionChanged(true, QStringLiteral("已连接：%1").arg(resource.trimmed()));
}

void Vxi11Client::disconnectDevice()
{
    closeSessions();
    emit connectionChanged(false, QStringLiteral("已断开。"));
}

bool Vxi11Client::writeBytes(const QByteArray &data, QString *message)
{
    if (!instrument_) {
        *message = QStringLiteral("尚未连接设备。");
        return false;
    }
    if (data.isEmpty()) {
        *message = QStringLiteral("写入内容为空。");
        return false;
    }
    if (quint64(data.size()) > std::numeric_limits<UInt32>::max()) {
        *message = QStringLiteral("写入内容过大。");
        return false;
    }
    UInt32 written = 0;
    const Status status = write_(instrument_,
        reinterpret_cast<const unsigned char *>(data.constData()), UInt32(data.size()), &written);
    if (status < 0) {
        *message = errorText(QStringLiteral("viWrite"), status);
        return false;
    }
    if (written != UInt32(data.size())) {
        *message = QStringLiteral("viWrite 仅发送了 %1/%2 字节。").arg(written).arg(data.size());
        return false;
    }
    *message = QStringLiteral("已发送 %1 字节。").arg(written);
    return true;
}

bool Vxi11Client::readBytes(QByteArray *data, QString *message)
{
    if (!instrument_) {
        *message = QStringLiteral("尚未连接设备。");
        return false;
    }
    data->clear();
    std::array<unsigned char, readChunkSize> buffer{};
    while (data->size() < maxResponseSize) {
        const UInt32 requested = UInt32(std::min<qint64>(readChunkSize,
                                                        qint64(maxResponseSize) - data->size()));
        UInt32 received = 0;
        const Status status = read_(instrument_, buffer.data(), requested, &received);
        if (received > requested) {
            *message = QStringLiteral("VISA 返回的字节数超过缓冲区大小。请检查 NI-VISA 版本。");
            return false;
        }
        data->append(reinterpret_cast<const char *>(buffer.data()), int(received));
        if (status < 0) {
            *message = errorText(QStringLiteral("viRead"), status);
            if (!data->isEmpty())
                *message += QStringLiteral("；已收到 %1 字节部分数据。").arg(data->size());
            return false;
        }
        if (status != successMaxCount) {
            *message = QStringLiteral("已接收 %1 字节。").arg(data->size());
            return true;
        }
        if (!received) {
            *message = QStringLiteral("VISA 报告缓冲区已满，但没有返回数据。");
            return false;
        }
    }
    *message = QStringLiteral("已接收 %1 字节，达到 1 MiB 显示上限，响应可能还有剩余数据。")
                   .arg(data->size());
    return true;
}

void Vxi11Client::writeData(const QByteArray &data)
{
    QString message;
    const bool ok = writeBytes(data, &message);
    emit operationFinished(QStringLiteral("写入"), ok, {}, message);
}

void Vxi11Client::readData()
{
    QByteArray data;
    QString message;
    const bool ok = readBytes(&data, &message);
    emit operationFinished(QStringLiteral("读取"), ok, data, message);
}

void Vxi11Client::query(const QByteArray &command)
{
    QString message;
    if (!writeBytes(command, &message)) {
        emit operationFinished(QStringLiteral("查询"), false, {}, message);
        return;
    }
    QByteArray data;
    const bool ok = readBytes(&data, &message);
    emit operationFinished(QStringLiteral("查询"), ok, data, message);
}

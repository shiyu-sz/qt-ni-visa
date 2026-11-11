#ifndef VXI11CLIENT_H
#define VXI11CLIENT_H

#include <QByteArray>
#include <QLibrary>
#include <QObject>
#include <QString>
#include <QtGlobal>

// All methods run in the worker thread. The DLL is loaded at runtime so the
// project can be built with MinGW without an NI import library or visa.h.
class Vxi11Client : public QObject
{
    Q_OBJECT
public:
    explicit Vxi11Client(QObject *parent = nullptr);

public slots:
    void connectDevice(const QString &resource, int timeoutMs);
    void disconnectDevice();
    void writeData(const QByteArray &data);
    void readData();
    void query(const QByteArray &data);

signals:
    void connectionChanged(bool connected, const QString &message);
    void operationFinished(const QString &operation, bool ok,
                           const QByteArray &data, const QString &message);

private:
    using Session = quint32;
    using Status = qint32;
    using UInt32 = quint32;
#ifdef Q_OS_WIN
#define VISA_CALL __stdcall
#else
#define VISA_CALL
#endif
    using OpenDefaultRM = Status (VISA_CALL *)(Session *);
    using Open = Status (VISA_CALL *)(Session, const char *, UInt32, UInt32, Session *);
    using Close = Status (VISA_CALL *)(Session);
    using SetAttribute = Status (VISA_CALL *)(Session, UInt32, quintptr);
    using Write = Status (VISA_CALL *)(Session, const unsigned char *, UInt32, UInt32 *);
    using Read = Status (VISA_CALL *)(Session, unsigned char *, UInt32, UInt32 *);
    using StatusDesc = Status (VISA_CALL *)(Session, Status, char *);
#undef VISA_CALL

    bool loadVisa(QString *error);
    void closeSessions();
    QString errorText(const QString &action, Status status) const;
    bool writeBytes(const QByteArray &data, QString *message);
    bool readBytes(QByteArray *data, QString *message);

    QLibrary library_;
    Session resourceManager_ = 0;
    Session instrument_ = 0;
    OpenDefaultRM openDefaultRM_ = nullptr;
    Open open_ = nullptr;
    Close close_ = nullptr;
    SetAttribute setAttribute_ = nullptr;
    Write write_ = nullptr;
    Read read_ = nullptr;
    StatusDesc statusDesc_ = nullptr;
};

#endif // VXI11CLIENT_H

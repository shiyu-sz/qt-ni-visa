#include "mainwindow.h"
#include "vxi11client.h"

#include <QCheckBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("NI-VISA VXI-11 客户端"));
    resize(780, 520);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *connectionBox = new QGroupBox(QStringLiteral("连接"), central);
    auto *connectionLayout = new QVBoxLayout(connectionBox);
    auto *form = new QFormLayout;
    resourceEdit_ = new QLineEdit(QStringLiteral("TCPIP0::192.168.1.100::inst1::INSTR"), connectionBox);
    resourceEdit_->setToolTip(QStringLiteral("将 192.168.1.100 改为服务器的实际 IP；inst1 改为服务器注册的设备名。"));
    form->addRow(QStringLiteral("VISA 资源名"), resourceEdit_);
    timeoutSpin_ = new QSpinBox(connectionBox);
    timeoutSpin_->setRange(100, 60000);
    timeoutSpin_->setValue(3000);
    timeoutSpin_->setSuffix(QStringLiteral(" ms"));
    form->addRow(QStringLiteral("超时"), timeoutSpin_);
    connectionLayout->addLayout(form);
    auto *connectionButtons = new QHBoxLayout;
    connectButton_ = new QPushButton(QStringLiteral("连接"), connectionBox);
    disconnectButton_ = new QPushButton(QStringLiteral("断开"), connectionBox);
    connectionButtons->addWidget(connectButton_);
    connectionButtons->addWidget(disconnectButton_);
    connectionButtons->addStretch();
    connectionLayout->addLayout(connectionButtons);
    layout->addWidget(connectionBox);

    auto *ioBox = new QGroupBox(QStringLiteral("数据收发"), central);
    auto *ioLayout = new QVBoxLayout(ioBox);
    commandEdit_ = new QLineEdit(ioBox);
    commandEdit_->setPlaceholderText(QStringLiteral("输入设备支持的命令；时钟示例可直接点击“读取”"));
    ioLayout->addWidget(commandEdit_);
    auto *ioButtons = new QHBoxLayout;
    newlineCheck_ = new QCheckBox(QStringLiteral("写入时追加换行符 (LF)"), ioBox);
    writeButton_ = new QPushButton(QStringLiteral("写入"), ioBox);
    readButton_ = new QPushButton(QStringLiteral("读取"), ioBox);
    queryButton_ = new QPushButton(QStringLiteral("查询（写入后读取）"), ioBox);
    ioButtons->addWidget(newlineCheck_);
    ioButtons->addStretch();
    ioButtons->addWidget(writeButton_);
    ioButtons->addWidget(readButton_);
    ioButtons->addWidget(queryButton_);
    ioLayout->addLayout(ioButtons);
    layout->addWidget(ioBox);

    log_ = new QPlainTextEdit(central);
    log_->setReadOnly(true);
    log_->setPlaceholderText(QStringLiteral("操作记录与设备响应"));
    layout->addWidget(log_, 1);
    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("未连接"));

    workerThread_ = new QThread(this);
    client_ = new Vxi11Client;
    client_->moveToThread(workerThread_);
    connect(workerThread_, &QThread::finished, client_, &QObject::deleteLater);
    connect(this, &MainWindow::connectRequested, client_, &Vxi11Client::connectDevice, Qt::QueuedConnection);
    connect(this, &MainWindow::disconnectRequested, client_, &Vxi11Client::disconnectDevice, Qt::QueuedConnection);
    connect(this, &MainWindow::writeRequested, client_, &Vxi11Client::writeData, Qt::QueuedConnection);
    connect(this, &MainWindow::readRequested, client_, &Vxi11Client::readData, Qt::QueuedConnection);
    connect(this, &MainWindow::queryRequested, client_, &Vxi11Client::query, Qt::QueuedConnection);
    connect(client_, &Vxi11Client::connectionChanged, this, [this](bool connected, const QString &message) {
        connected_ = connected;
        setBusy(false);
        statusBar()->showMessage(message);
        appendLog(message);
    });
    connect(client_, &Vxi11Client::operationFinished, this,
            [this](const QString &operation, bool ok, const QByteArray &data, const QString &message) {
        setBusy(false);
        appendLog(QStringLiteral("%1：%2").arg(operation, message));
        if (!data.isEmpty()) {
            const QString decoded = QString::fromUtf8(data);
            if (!data.contains('\0') && decoded.toUtf8() == data)
                appendLog(QStringLiteral("响应：%1").arg(decoded));
            else
                appendLog(QStringLiteral("响应 (HEX)：%1").arg(QString::fromLatin1(data.toHex(' '))));
        }
        if (!ok)
            statusBar()->showMessage(message, 10000);
    });
    workerThread_->start();

    connect(connectButton_, &QPushButton::clicked, this, [this] {
        setBusy(true);
        appendLog(QStringLiteral("连接：%1").arg(resourceEdit_->text().trimmed()));
        emit connectRequested(resourceEdit_->text(), timeoutSpin_->value());
    });
    connect(disconnectButton_, &QPushButton::clicked, this, [this] {
        setBusy(true);
        emit disconnectRequested();
    });
    connect(writeButton_, &QPushButton::clicked, this, [this] {
        setBusy(true);
        emit writeRequested(commandBytes());
    });
    connect(readButton_, &QPushButton::clicked, this, [this] {
        setBusy(true);
        emit readRequested();
    });
    connect(queryButton_, &QPushButton::clicked, this, [this] {
        setBusy(true);
        emit queryRequested(commandBytes());
    });
    setBusy(false);
    appendLog(QStringLiteral("请先填写服务器 IP 和设备名，然后连接。时钟示例连接后直接点击“读取”。"));
}

MainWindow::~MainWindow()
{
    QMetaObject::invokeMethod(client_, "disconnectDevice", Qt::BlockingQueuedConnection);
    workerThread_->quit();
    workerThread_->wait();
}

void MainWindow::setBusy(bool busy)
{
    resourceEdit_->setEnabled(!busy && !connected_);
    timeoutSpin_->setEnabled(!busy && !connected_);
    connectButton_->setEnabled(!busy && !connected_);
    disconnectButton_->setEnabled(!busy && connected_);
    commandEdit_->setEnabled(!busy && connected_);
    newlineCheck_->setEnabled(!busy && connected_);
    writeButton_->setEnabled(!busy && connected_);
    readButton_->setEnabled(!busy && connected_);
    queryButton_->setEnabled(!busy && connected_);
}

QByteArray MainWindow::commandBytes() const
{
    QByteArray data = commandEdit_->text().toUtf8();
    if (newlineCheck_->isChecked() && !data.endsWith('\n'))
        data.append('\n');
    return data;
}

void MainWindow::appendLog(const QString &message)
{
    const QString time = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    log_->appendPlainText(QStringLiteral("[%1] %2").arg(time, message));
}

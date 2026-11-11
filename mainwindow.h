#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QByteArray>

class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QThread;
class Vxi11Client;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void connectRequested(const QString &resource, int timeoutMs);
    void disconnectRequested();
    void writeRequested(const QByteArray &data);
    void readRequested();
    void queryRequested(const QByteArray &data);

private:
    void setBusy(bool busy);
    QByteArray commandBytes() const;
    void appendLog(const QString &message);

    QLineEdit *resourceEdit_ = nullptr;
    QSpinBox *timeoutSpin_ = nullptr;
    QLineEdit *commandEdit_ = nullptr;
    QCheckBox *newlineCheck_ = nullptr;
    QPushButton *connectButton_ = nullptr;
    QPushButton *disconnectButton_ = nullptr;
    QPushButton *writeButton_ = nullptr;
    QPushButton *readButton_ = nullptr;
    QPushButton *queryButton_ = nullptr;
    QPlainTextEdit *log_ = nullptr;
    QThread *workerThread_ = nullptr;
    Vxi11Client *client_ = nullptr;
    bool connected_ = false;
};
#endif // MAINWINDOW_H

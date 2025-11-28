#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QKeyEvent>
#include "lockworker.h"
#include <QRect>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
public slots: // 接收信号的槽函数
    int handleLockRequest();
    void safeHideWindow() {
        this->hide();
        qDebug() << "Window safely hidden by Main Thread.";
        QCoreApplication::quit();
    }
protected:
    void keyPressEvent(QKeyEvent *event) override;
private:
    Ui::MainWindow *ui;
    LockWorker *m_worker = nullptr;
    std::optional<std::thread> m_workerThread;
};
#endif // MAINWINDOW_H

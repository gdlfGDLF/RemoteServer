#include "lockworker.h"
#include <QThread> // 引入 QThread 头文件
std::atomic<bool> g_isLocked = false;
LockWorker::LockWorker(QObject *parent) : QObject(parent)
{

}
// 实现线程的主循环
void LockWorker::doLockWait()
{
    qDebug() << "Worker thread started, waiting for unlock signal...";

    // LockMachine 的逻辑：持续检查 g_isLocked 标志
    while (g_isLocked.load())
    {
        //qDebug()<<"cheak";
        // 避免 CPU 满载
        QThread::msleep(50);
    }
    qDebug() << "Unlock flag received. Sending GUI close signal...";

    emit unlockComplete();
    this->deleteLater();
}

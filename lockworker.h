#ifndef LOCKWORKER_H
#define LOCKWORKER_H
#include <QObject>
#include <QDebug>
#include <thread>
#include <atomic>
extern std::atomic<bool> g_isLocked;

class LockWorker :public QObject
{
    Q_OBJECT // 必须有，才能使用信号/槽
public:
    explicit LockWorker(QObject *parent = nullptr);
public slots:
    // 线程启动后，在这个槽函数中执行你的核心循环
    void doLockWait();

signals:
    // 关键信号：当检测到解锁标志时发出
    void unlockComplete();
};

#endif // LOCKWORKER_H

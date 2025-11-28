#include "mainwindow.h"
#include "Serversocket.h"
#include"qapplication.h"
#include"serverlistener.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    CServersocket& server=CServersocket::GetInstance();
    MainWindow w;
    std::unique_ptr<LockWorker> workerPtr(new LockWorker());
    LockWorker *worker = workerPtr.get();
    //信号和槽
    //connect （发射信号的对象，信号，接受对象，执行的槽函数）
    QObject::connect(worker, &LockWorker::unlockComplete,
                     &w, &MainWindow::safeHideWindow,
                     Qt::QueuedConnection);
    QObject::connect(&server, &CServersocket::lockCommandReceived,
                     &w, &MainWindow::handleLockRequest,
                     Qt::QueuedConnection);
    //QMetaObject::invokeMethod(&w, "handleLockRequest", Qt::QueuedConnection);
    //QMetaObject::invokeMethod(&w, "handleLockRequest", Qt::DirectConnection);
    w.setAttribute(Qt::WA_DeleteOnClose); // 窗口关闭时自动释放内存
    ServerListener *listener = new ServerListener(); // 创建 Worker
    // 使用 std::thread 启动 listener->listenLoop()
    std::thread listenerThread([listener]{
        // 启动 listener 对象的槽函数
        QMetaObject::invokeMethod(listener, "listenLoop", Qt::DirectConnection);
    });
    // 2. 检查并分离线程
    if (!listenerThread.joinable()) { return -1; }
    qDebug()<<"deatch!";
    listenerThread.detach();

    return a.exec();
}


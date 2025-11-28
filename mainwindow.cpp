#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <windows.h>
static HHOOK g_keyboardHook = NULL;//暂时无用
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::FramelessWindowHint);
    //this->setwindow
}

MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // 检查按下的键是否是 Escape 键
    if (event->key() == Qt::Key_Escape)
    {
        qDebug() << "Escape key pressed, closing window...";
        QCoreApplication::quit();
    }
    else
    {
        QMainWindow::keyPressEvent(event);
    }
}

int MainWindow::handleLockRequest()
{
    if (m_workerThread.has_value() && m_workerThread->joinable()) {
        m_workerThread.reset();
    }
    if(this->isVisible())
    {
        qDebug()<<"visble 存在";
        return 1;
    }
    qDebug()<<"<<";
    g_isLocked.store(true);

    this->showFullScreen();
    HWND hWnd = (HWND)this->winId();
    SetWindowPos(
        hWnd,               // 目标窗口句柄
        HWND_TOPMOST,       // 将窗口置于所有非 TOPMOST 窗口之上
        0, 0, 0, 0,         // X, Y, Width, Height (NOMOVE 和 NOSIZE 忽略这些值)
        SWP_NOMOVE |        // 保持当前位置不变
        SWP_NOSIZE          // 保持当前尺寸不变
        );
    if(this->winId()==0)
    {
        return 1;
    }
    if (m_worker!=NULL) {
        delete m_worker;
        m_worker = nullptr;
    }
    qDebug()<<m_worker;
    m_worker = new LockWorker(this);
    qDebug()<<m_worker;
    QObject::connect(m_worker, &LockWorker::unlockComplete,
                     this, &MainWindow::safeHideWindow,
                     Qt::QueuedConnection);
    //鼠标锁逻辑
    ::ShowCursor(FALSE);
    QRect geometry = this->geometry();
    RECT clipRect;
    clipRect.left = geometry.left();
    clipRect.top = geometry.top();
    clipRect.right = geometry.right();
    clipRect.bottom = geometry.bottom();
    ClipCursor(&clipRect);
    m_workerThread.emplace([worker = m_worker]{
        QMetaObject::invokeMethod(worker, "doLockWait", Qt::DirectConnection);
    });
    if(!m_workerThread->joinable()) {
        // ... 错误处理 ...
        m_workerThread.reset();
        return -1;
    }
    m_workerThread->detach();
    qDebug() << "New lock thread launched successfully.";
    return 1;
}
//阻止键盘事件

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        // lParam 包含了键盘事件的信息
        KBDLLHOOKSTRUCT *pKeyStruct = (KBDLLHOOKSTRUCT *)lParam;

        // 检查 Windows 键或 Alt+Tab

        // A. 拦截 Windows 键 (左/右 Windows 键)
        if (pKeyStruct->vkCode == VK_LWIN || pKeyStruct->vkCode == VK_RWIN) {
            return 1; // 返回 1，阻止事件继续传递 (拦截)
        }

        // B. 拦截 Alt+Tab 和 Alt+Esc
        // (wParam == WM_SYSKEYDOWN 发生在 Alt 键按下时)
        if (wParam == WM_SYSKEYDOWN) {
            // 检查 Tab 或 Esc
            if (pKeyStruct->vkCode == VK_TAB || pKeyStruct->vkCode == VK_ESCAPE) {
                return 1; // 拦截 Alt+Tab 和 Alt+Esc
            }
        }
    }
    // 如果不拦截，将事件传递给钩子链中的下一个程序
    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

void installKeyboardHook() {
    // 1. 获取钩子函数（KeyboardProc）的地址
    HMODULE hMod = GetModuleHandle(NULL);

    // 2. 安装低级键盘钩子 (WH_KEYBOARD_LL)
    // 0 表示全局钩子，NULL 表示没有特定的线程 ID
    g_keyboardHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,            // 钩子类型：低级键盘
        KeyboardProc,              // 钩子回调函数的地址
        hMod,
        0                          // 0 表示全局，作用于所有线程
        );

    if (g_keyboardHook == NULL) {
        qDebug() << "致命错误：键盘钩子安装失败！";
    }
}
void uninstallKeyboardHook() {
    if (g_keyboardHook != NULL) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = NULL;
        qDebug() << "键盘钩子已安全移除。";
    }
}

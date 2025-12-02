#include "serverlistener.h"
#include<io.h>
#include <QApplication>
#include "Serversocket.h"
#include "direct.h"
#include"QScreen"
#include <shellapi.h>
#include <QDateTime>
#include <qdatastream.h>
#include <qbuffer.h>
#include<lockworker.h>
#include <filesystem>
#pragma pack(1)
struct FilePacketHeader {
    // 使用 uint8_t 确保大小是 1 字节，且无符号
    uint8_t isDirectory;
    uint8_t hasChildren;
    // 使用 uint32_t 确保大小是 4 字节，用于存储文件名长度
    uint32_t nameLength;
};
#pragma pack()


void Dump(const BYTE* pData, size_t nSize);
int MakeDirverInfo();
int MakeDirectoryInfo();
int DirectioryInfo();
int RunFile();
int DownLoad();
int MouseEVent();
int SendScreen();
int TestConnect();


int execute(int cmd,CServersocket& server)
{
    int nCmd=cmd;
    qDebug()<<"recive cmd:"<<cmd;
    int ret =1;
    switch (nCmd) {
        case 1://看查盘符
            ret=MakeDirverInfo();
            break;
        case 2://看下指定目录下文件
            ret=DirectioryInfo();
            break;
        case 3://打开
            ret=RunFile();
            break;
        case 4://下载
            ret=DownLoad();
            break;
        case 5://删除
            break;
        case 6://鼠标操作
            //ret=MouseEvent();
            break;
        case 7://看查屏幕
            ret=SendScreen();
            break;
        case 8://锁机
            g_isLocked.store(true);
            emit server.lockCommandReceived();
            break;
        case 9://解锁
            g_isLocked.store(FALSE);
            break;
        case 12138:
            qDebug()<<"12138!";
            TestConnect();
        default:
            //qDebug()<<"1";
            ret=-99;
        }
    return ret;
}
void ServerListener::listenLoop() {
    // 获取单例对象
    qDebug()<<"listenLoop！";
    CServersocket& server = CServersocket::GetInstance();
    if(server.StartListen()==false)
    {
        qDebug()<<"StartListen=flase！";
        return;
    }
    while(true)
    {
        if(server.AcceptClient()==INVALID_SOCKET)
        {
            qDebug()<<"AcceptClient=flase！";
            break;
        }
        qDebug()<<"AcceptClient=true！";
        int ret=server.DealCommand();
        qDebug()<<"ret:"<<ret;
        int exeBack=execute(ret,server);
        if(exeBack == -1)
        {
            qDebug()<<"命令错误！";
        }
        server.closeLink();
    }
    qDebug()<<"退出linstenLOOp";
    return ;
}

void Dump(const BYTE* pData, size_t nSize)
{
    std::string sout;
    char buf[8] = "";
    sout.reserve(nSize * 3 + (nSize / 16) + 10);
    if (!pData || nSize == 0) return;
    for (size_t i = 0; i < nSize; i++) {
        if (i > 0 && (i % 16 == 0)) {
            sout += "\n";
        }
        snprintf(buf, sizeof(buf), "%02hhX ", (unsigned char)pData[i]);
        sout += buf;
    }
    sout += "\n";
    qDebug() <<"dump!"<< sout.c_str();
}
int MakeDirverInfo()
{
    qDebug()<<"MakeDi running";
    std::string result;
    for (int i = 1; i <= 26; i++) {
        if(_chdrive(i)==0){
            if(result.size()>0){
                result+=',';
            }
            result+='A'+i-1;
        }
    }
    //result+='/0';
    SPackeg pack(1,(BYTE*)result.c_str(),result.size()+1);
    Dump((BYTE*)pack.getPacketBuffer().data(),pack.nLength);
    CServersocket::GetInstance().Send(pack);
    return 0;
}
namespace fs = std::filesystem; // 简化命名空间
bool hasChildren(fs::directory_entry entry);
/*
int DirectioryInfo()
{
    SPackeg currentPacket = CServersocket::GetInstance().GetPackeg();
    std::string pathString = currentPacket.strData.data();
    fs::path DirPath = pathString;
    const WORD CMD_FILE_LIST = 2;
    FilePacketHeader header;
    std::string entryPathStr;
    QByteArray payload;
    try {
        for (const auto& entry : fs::directory_iterator(DirPath)) {
            fs::path filename = entry.path().filename();
            std::string entryFileNameStr{};
            if (filename == "." || filename == "..") {
                continue; // 跳过本次循环
            }
            if (entry.is_regular_file() || entry.is_directory()) {
                // 【正式获取文件名】
                entryFileNameStr = filename.u8string();
            }
            header.isDirectory = entry.is_directory();
            //定义结构体
            if(header.isDirectory)
            {
                //如果是文件夹 判断有没有内容;
                header.hasChildren = hasChildren(entry);
            }
            else
            {
                //如果是文件 那肯定没有子文件
                header.hasChildren =false;
            }
            header.nameLength = (uint32_t)entryFileNameStr.size();

            //计算长度 *****  信息头+文件名
            const size_t HEADER_SIZE = sizeof(FilePacketHeader);
            size_t totalPayloadSize = HEADER_SIZE + entryFileNameStr.size();//总长度
            payload.resize(totalPayloadSize);
            qDebug()<<header.nameLength;
            char* cursor = payload.data();
            memcpy(cursor, &header, HEADER_SIZE);
            cursor += HEADER_SIZE;
            memcpy(cursor, entryFileNameStr.data(), entryFileNameStr.size());

            SPackeg pack(CMD_FILE_LIST,
                         (BYTE*)payload.constData(),
                         payload.size());
            qDebug()<<"send fileinfo ";
            CServersocket::GetInstance().Send(pack);
        }
    }
    catch (const fs::filesystem_error& e) {
        qDebug() << "文件系统错误: " << e.what();
        SPackeg errPack(-999,NULL,0);
        CServersocket::GetInstance().Send(errPack);
        return -1;
    }
    SPackeg endPack(-1, nullptr, 0);//暂时代表结束
    CServersocket::GetInstance().Send(endPack);
    return 1;
}*/
int DirectioryInfo()
{
    SPackeg currentPacket = CServersocket::GetInstance().GetPackeg();
    std::string pathString = currentPacket.strData.data();
    fs::path DirPath = pathString;

    // 其他变量初始化
    const WORD CMD_FILE_LIST = 2;
    FilePacketHeader header;
    std::string entryPathStr;
    QByteArray payload;


    std::error_code ec; // 声明 error_code 对象
    fs::directory_iterator it(DirPath, ec);
    if (ec) {
        qDebug() << "文件系统错误：无法访问路径 " << DirPath.string().c_str()
                 << "。原因: " << ec.message().c_str();
        SPackeg errPack(-999, NULL, 0);
        CServersocket::GetInstance().Send(errPack);
        return -1;
    }
    while (it != fs::directory_iterator())
    {
        // 1. 获取当前 entry
        const auto& entry = *it;

        fs::path filename = entry.path().filename();
        std::string entryFileNameStr{};

        if (filename == "." || filename == "..") {
            // 在递增前 continue，确保跳过当前 entry 的后续逻辑
            it.increment(ec);
            if (ec) { /* 忽略递增时的潜在错误，等待下一轮处理 */ ec.clear(); }
            continue;
        }

        entryFileNameStr = filename.u8string();
        qDebug()<<entryFileNameStr;
        header.isDirectory = entry.is_directory();
        if(header.isDirectory) {
            header.hasChildren = hasChildren(entry); // 假设 hasChildren 不会抛异常
        } else {
            header.hasChildren = false;
        }
        header.nameLength = (uint32_t)entryFileNameStr.size();

        const size_t HEADER_SIZE = sizeof(FilePacketHeader);
        size_t totalPayloadSize = HEADER_SIZE + entryFileNameStr.size();
        payload.resize(totalPayloadSize);
        //qDebug()<<header.nameLength;
        char* cursor = payload.data();
        memcpy(cursor, &header, HEADER_SIZE);
        cursor += HEADER_SIZE;
        memcpy(cursor, entryFileNameStr.data(), entryFileNameStr.size());

        SPackeg pack(CMD_FILE_LIST,
                     (BYTE*)payload.constData(),
                     payload.size());
        //qDebug()<<"send fileinfo ";
        CServersocket::GetInstance().Send(pack);
        // 2. 关键步骤：递增迭代器并检查错误
        it.increment(ec);
        if (ec) {
            // 遇到不可访问的目录项或文件时，不退出循环
            qDebug() << "警告：跳过不可访问的目录项。原因: " << ec.message().c_str();
            ec.clear(); // 清除错误状态，确保下一次迭代可以尝试继续
        }
    }
    SPackeg endPack(-1, nullptr, 0);//暂时代表结束
    CServersocket::GetInstance().Send(endPack);
    return 1;
}
bool hasChildren(fs::directory_entry entry)
{
    std::error_code ec; // 声明 error_code 对象

    // 1. 尝试构造迭代器，并传入 ec。如果目录不可访问，it 的构造会失败，
    //    但不会抛出异常，而是将 ec 设置为权限错误。
    fs::directory_iterator it(entry.path(), ec);
    if (ec) {
         qDebug() << "hasChildren 警告：无法访问目录 " << entry.path().string().c_str()<< "。原因: " << ec.message().c_str();
        return false;
    }
    // 3. 检查目录是否为空
    // 如果构造成功且 it != fs::directory_iterator()，则目录非空
    // 还需要跳过 "." 和 ".."
    for (auto const& dir_entry : it)
    {
        // 过滤掉 '.' 和 '..'
        if (dir_entry.path().filename() != "." && dir_entry.path().filename() != "..") {
            return true; // 找到了至少一个子项，返回 true
        }
    }

    return false;
}






int RunFile()
{
    std::string strPath;
    CServersocket::GetInstance().GetFilePath(strPath);
    ShellExecuteA(NULL,NULL,strPath.c_str(),NULL,NULL,SW_SHOWNORMAL);
    SPackeg pack((WORD)3,NULL,0);
    CServersocket::GetInstance().Send(pack);
    return 0;
}
int DownLoad()
{
    std::string strPath;
    long long data=0;
    CServersocket::GetInstance().GetFilePath(strPath);
    qDebug()<<strPath;
    FILE* pFile=NULL;
    errno_t err=fopen_s(&pFile,strPath.c_str(),"rb");
    if(err!=0){
        SPackeg pack(100,NULL,0);//打开失败直接发错误
        CServersocket::GetInstance().Send(pack);
        return -1;
    }
    if(pFile!=NULL){
        fseek(pFile,0,SEEK_END);
        data=_ftelli64(pFile);
        qDebug()<<"filesize:"<<data;
        const int MAX_PAYLOAD = 1024;
        int num_packets = (data + MAX_PAYLOAD - 1) / MAX_PAYLOAD;
        long long total_bytes_to_send = data + (long long)num_packets * 10 + 28;

        SPackeg head(15,(BYTE*)&total_bytes_to_send,8); //判断文件大小 发出去第一个包 让客户端确认大小
        qDebug()<<"head.sinze:"<<head.nLength;
        CServersocket::GetInstance().Send(head);
        fseek(pFile,0,SEEK_SET);

        char buffer[1024]="";
        size_t rlen=0;
        do{
            rlen=fread(buffer,1,1024,pFile);
            SPackeg pack(4,(BYTE*)buffer,rlen);
            //qDebug() << "Data Packet nLength Check:" << pack.nLength; // 期望输出 64
            // 立即在发送前再次检查其 buffer 中的字节：
            //const char* data_ptr = pack.getPacketBuffer().data();
            //qDebug()<<"send pack length:"<<pack.nLength;
            //qDebug()<<"iiiiiiiiiiiiiiiii:"<<i++;
            // qDebug() << "Data Packet Raw nLength Bytes Before Send:"
            //          << (int)data_ptr[2] << (int)data_ptr[3] << (int)data_ptr[4] << (int)data_ptr[5];
            //qDebug()<<"packlength:"<<sizeof(pack.getPacketBuffer());
            CServersocket::GetInstance().Send(pack);
        }while(rlen==1024);
    }
    SPackeg pack((WORD)99,NULL,0); //99表示发包结束了
    qDebug()<<"sned end pack!";
    CServersocket::GetInstance().Send(pack);
    //qDebug()<<"end pack size"<<pack.nLength;
    fclose(pFile);
    return 1;
}
int MouseEVent()
{
    MouseEvent mouse;
    // 1. 获取鼠标事件数据
    if (!CServersocket::GetInstance().GetMouseEvent(mouse))
    {
        qDebug() << "获取失败";
        return -1;
    }

    // 2. 移动光标 (SetCursorPos 是独立的，始终执行)
    // 鼠标移动操作通常无需单独的 nAction 标志即可执行
    if (mouse.nButton == 4) // nButton=4 表示移动指令
    {
        SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);
        // 如果只是移动，不需要后续 mouse_event 调用
        return 1;
    }

    // --- 3. 处理点击/按下/松开事件 ---

    // 3.1 移动光标到目标位置 (确保点击发生在正确位置)
    SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);

    // 3.2 定义操作类型 (DOWN, UP)
    DWORD down_flag = 0;
    DWORD up_flag = 0;

    // 3.3 映射按钮 (nButton)
    switch (mouse.nButton)
    {
    case 0: // 左键
        down_flag = MOUSEEVENTF_LEFTDOWN;
        up_flag = MOUSEEVENTF_LEFTUP;
        break;
    case 1: // 右键
        down_flag = MOUSEEVENTF_RIGHTDOWN;
        up_flag = MOUSEEVENTF_RIGHTUP;
        break;
    case 2: // 中键
        down_flag = MOUSEEVENTF_MIDDLEDOWN;
        up_flag = MOUSEEVENTF_MIDDLEUP;
        break;
    default:
        return 0; // 不支持的按钮
    }

    // --- 4. 执行操作 (nAction) ---

    switch (mouse.nAction)
    {
    case 2: // 按下 (DOWN)
    case 0: // 单击 (Click) - 单击首先是按下
        mouse_event(down_flag, 0, 0, 0, GetMessageExtraInfo());

        if (mouse.nAction == 2) {
            // 如果是按住不放，则到此为止
            break;
        }
        // Fall-through for click (松开操作紧随其后)

    case 3: // 松开 (UP)
        mouse_event(up_flag, 0, 0, 0, GetMessageExtraInfo());
        break;
    case 1: // 双击 (Double Click)
        // 执行两次完整的按下-松开序列
        mouse_event(down_flag, 0, 0, 0, GetMessageExtraInfo());
        mouse_event(up_flag, 0, 0, 0, GetMessageExtraInfo());
        mouse_event(down_flag, 0, 0, 0, GetMessageExtraInfo());
        mouse_event(up_flag, 0, 0, 0, GetMessageExtraInfo());
        break;
    }

    // 5. 返回确认包 (假设命令ID=4)
    SPackeg pack((WORD)5, NULL, 0);
    CServersocket::GetInstance().Send(pack);

    return 1;
}

int SendScreen()
{
    /*
    // 1. 获取主屏幕对象
    QScreen *screen = QGuiApplication::primaryScreen();

    // 2. 捕获整个窗口 (Windows 上的 '0' 通常指整个桌面)
    QPixmap screenshot = screen->grabWindow(0);

    // 3. 生成一个基于时间的唯一文件名，避免覆盖
    QString fileName = QString("screenshot_%1.jpg").arg(QDateTime::currentDateTime().toString("hhmmss"));

    // 4. 将 QPixmap 保存为 JPG 文件
    if (screenshot.save(fileName, "JPG", 85)) {
        qDebug() << "✅ 截图已成功保存到文件：" << fileName;
        return 1;
    } else {
        qDebug() << "❌ 截图保存失败！请检查文件写入权限。";
        return -1;
    }
    */

    QScreen *screen = QGuiApplication::primaryScreen();
    QPixmap screenshot = screen->grabWindow(0);
    QByteArray imageData; // 最终要发送的字节流
    QBuffer buffer(&imageData); // 将 QByteArray 绑定到 QBuffer
    buffer.open(QIODevice::WriteOnly);
    if (screenshot.save(&buffer, "JPG", 80)) {
        qDebug() << "Image serialized successfully. Size:" << imageData.size();
        const WORD CMD_SCREENSHOT = 5;
        SPackeg pack(
            CMD_SCREENSHOT,
            (const BYTE*)imageData.constData(), // 获取 QByteArray 的原始数据指针
            imageData.size()                    // 获取实际数据长度
            );
        CServersocket::GetInstance().Send(pack);
        return 1;
    } else {
        qDebug() << "Image serialization failed.";
        return -1;
    }
}


int TestConnect()
{
    qDebug()<<"testconnect0";
    SPackeg pack(12138,NULL,0);
    CServersocket::GetInstance().Send(pack);
    return 0;
}

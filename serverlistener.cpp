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
typedef struct file_info{
    file_info(){
        IsInvalid=FALSE;
        IsDriectory=-1;
        HasNext=TRUE;
        memset(szFileName,0,sizeof(szFileName));
    }
    BOOL IsInvalid;//是否有效
    BOOL IsDriectory;//是否是目录 0否1是
    BOOL HasNext; //是否有后续 0无 1 有
    char szFileName[256];//文件名


}FILEINFO,*PFILEINFO;
void Dump(const BYTE* pData, size_t nSize);
int MakeDirverInfo();
int MakeDirectoryInfo();
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
            ret=MakeDirectoryInfo();
            break;
        case 3://打开
            ret=RunFile();
            break;
        case 4://下载
            ret=DownLoad();
            break;
        case 5://鼠标操作
            //ret=MouseEvent();
            break;
        case 6://看查屏幕
            ret=SendScreen();
            break;
        case 7://锁机
            g_isLocked.store(true);
            emit server.lockCommandReceived();
            break;
        case 8://解锁
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
    SPackeg pack(1,(BYTE*)result.c_str(),result.size());
    Dump((BYTE*)pack.getPacketBuffer().data(),pack.nLength);
    CServersocket::GetInstance().Send(pack);
    return 0;
}

int MakeDirectoryInfo()
{

    SPackeg currentPacket = CServersocket::GetInstance().GetPackeg();
    std::string strPath = currentPacket.strData.data();
    qDebug()<<"MakeDirinfo!:"<<strPath;
    if(CServersocket::GetInstance().GetFilePath(strPath)==false)
    {
        qDebug()<<"当前命令错误 GetFilePath!";
        return -1;
    }
    if(_chdir(strPath.c_str())!=0)
    {
        FILEINFO finfo;
        finfo.IsInvalid=TRUE;
        finfo.IsDriectory=TRUE;
        finfo.HasNext=FALSE;
        size_t len = strPath.size();
        if (len < sizeof(finfo.szFileName)) { // 确保不越界
            memcpy(finfo.szFileName, strPath.c_str(), len + 1); // +1 复制 \0
        }
        //lstFileInfos.push_back(finfo);
        SPackeg pack((WORD)2,(BYTE*)&finfo,sizeof(finfo));
        CServersocket::GetInstance().Send(pack);

        qDebug()<<"无权限访问目录";
        return -2;
    }
    _finddata_t fdata;
    int hfind =0;
    if((hfind=_findfirst("*",&fdata))==-1)
    {
        qDebug()<<"未找到任何文件";
        return -3;
    }
    do{
        FILEINFO finfo;
        finfo.IsDriectory = (fdata.attrib &_A_SUBDIR)!=0;
        memcpy(finfo.szFileName,fdata.name,strlen(fdata.name));
        //lstFileInfos.push_back(finfo);
        SPackeg pack((WORD)2,(BYTE*)&finfo,sizeof(finfo));
        CServersocket::GetInstance().Send(pack);
    }while(!_findnext(hfind,&fdata));
    FILEINFO finfo;
    finfo.HasNext=FALSE;
    SPackeg pack((WORD)2,(BYTE*)&finfo,sizeof(finfo));
    CServersocket::GetInstance().Send(pack);
    return 0;
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
    FILE* pFile=NULL;
    errno_t err=fopen_s(&pFile,strPath.c_str(),"rb");
    if(err!=0){
        SPackeg pack(4,(BYTE*)&data,8);
        CServersocket::GetInstance().Send(pack);
        return -1;
    }
    if(pFile==NULL){
        fseek(pFile,0,SEEK_END);
        data=_ftelli64(pFile);
        SPackeg head(4,(BYTE*)&data,8);
        fseek(pFile,0,SEEK_SET);

        char buffer[1024]="";
        size_t rlen=0;
        do{
            rlen=fread(buffer,1,1024,pFile);
            SPackeg pack(4,(BYTE*)buffer,rlen);
            CServersocket::GetInstance().Send(pack);
        }while(rlen==1024);
    }
    SPackeg pack((WORD)4,NULL,0);
    CServersocket::GetInstance().Send(pack);
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

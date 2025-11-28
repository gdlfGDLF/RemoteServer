#ifndef SERVERSOCKET_H
#define SERVERSOCKET_H
#include "QDebug"
#include "WinSock2.h"
#include <vector>
#include <cstring>
#include <QObject>
#pragma comment(lib, "ws2_32.lib")
typedef struct MouseEvent{
    MouseEvent(){
        nAction=0;
        nButton=-1;
        ptXY.x=0;
        ptXY.y=0;
    }
    WORD nAction; //点击 移动 双击
    WORD nButton; //左键 右键 中键
    POINT ptXY;//坐标
}MOUSEV,*PMOUSEV;
//TODO:对收到的包进行定义
class SPackeg
{
public:
    SPackeg():sHead(0),nLength(0),sCmd(0),sSum(0){};
    SPackeg(const SPackeg& pack){
        sHead=pack.sHead;
        nLength=pack.nLength;
        sCmd=pack.sCmd;
        strData=pack.strData;
        sSum=pack.sSum;
    }
    //打包
    SPackeg(WORD nCmd,const BYTE* data,size_t nSize) //nSize=data长度 head+2 length+4 cmd+2 sum+2
    {
        qDebug()<<"打包";
        sHead=0XFEFF;
        nLength=nSize+10;
        sCmd=nCmd;
        if(nSize>0)
        {
            strData.resize(nSize);
            memcpy(strData.data(),data,nSize);
            //对象内存地址虽然连续 但是因为对齐问题 对象数据之间需要填充
            //打包成连续的内容
            this->packetBuffer.resize(nLength);
            char* cursor = this->packetBuffer.data();
            memcpy(cursor, &sHead, sizeof(WORD));
            cursor += sizeof(sHead);
            memcpy(cursor, &nLength, sizeof(DWORD));
            cursor += sizeof(nLength);
            memcpy(cursor, &sCmd, sizeof(WORD));
            cursor += sizeof(sCmd);
            memcpy(cursor, strData.data(), strData.size());
            cursor += strData.size();
            memcpy(cursor, &sSum, sizeof(WORD));
            //qDebug()<<"packet:"<<packetBuffer.data();
        }
        else{
            strData.clear();
        }
        sSum=0;
        for (size_t j = 0; j < strData.size(); j++) {
            sSum += (unsigned char)strData[j];
        }
    }
    //解包
    SPackeg(const BYTE* pData,size_t& nSize){
        qDebug()<<"解包开始";

        // 假设这些是类的成员变量，我们只修改局部逻辑
        const BYTE* cursor = pData;
        size_t i = 0;

        // 1. 包头寻找 (如果包头没对齐)
        // 寻找逻辑保持不变，但现在 i 代表包头起始偏移量，cursor 指向 nLength
        WORD temp_head = 0;
        for (; i < nSize - sizeof(WORD); i++) {
            memcpy(&temp_head, pData + i, sizeof(WORD));
            if (temp_head == 0XFEFF) {
                cursor = pData + i + sizeof(WORD); // 游标指向 nLength
                sHead = temp_head;
                break;
            }
        }

        // 2. 检查是否找到包头，并确保缓冲区足够容纳固定头部
        if (sHead != 0XFEFF || (i + 10) > nSize) {
            // 未找到包头，或者连固定头部都接收不完整
            nSize = 0; // 告知外部未消耗数据
            return;
        }

        // 3. 读取 nLength (总长度)
        memcpy(&nLength, cursor, sizeof(DWORD));
        cursor += sizeof(DWORD);

        // 4. 【关键检查】确认数据包是否完整地在缓冲区中
        if (i + nLength > nSize) {
            // 数据包不完整，等待更多数据
            nSize = 0;
            return;
        }

        // 5. 读取 sCmd (命令)
        qDebug()<<"befor"<<sCmd;
        memcpy(&sCmd, cursor, sizeof(WORD));
        qDebug()<<"after"<<sCmd;
        cursor += sizeof(WORD);

        // 6. 计算 Payload 大小
        const int FIXED_HEADER_SIZE = sizeof(WORD) + sizeof(DWORD) + sizeof(WORD) + sizeof(WORD);
        int payload_size = nLength - FIXED_HEADER_SIZE;

        // 7. 读取 Payload (修正写入错误)
        strData.resize(payload_size);
        if (payload_size > 0) {
            // ✅ 修正 UB：使用 data() 写入实际缓冲区
            memcpy(strData.data(), cursor, payload_size);
        }
        cursor += payload_size; // 游标向前移动载荷长度

        // 8. 读取 sSum (校验和)
        memcpy(&sSum, cursor, sizeof(WORD));
        cursor += sizeof(WORD);

        // 9. 校验和计算 (逻辑不变)
        WORD calculated_sum = 0;
        // ... (校验逻辑) ...
        if (calculated_sum != sSum) {
            qDebug() << "校验和不匹配，数据包可能已损坏！";
            nSize = 0;
            return;
        }

        // 10. 告知外部函数：成功解包，并消耗了多少字节
        nSize = cursor - pData; // ✅ 返回消耗的字节数
    }
    ~SPackeg(){

    };
    SPackeg& operator=(const SPackeg& pack){
        if(this != &pack)
        {
        sHead=pack.sHead;
        nLength=pack.nLength;
        sCmd=pack.sCmd;
        strData=pack.strData;
        sSum=pack.sSum;
        }
        return *this;
    }

    const std::vector<char>& getPacketBuffer() const
    {
        return packetBuffer;
    }

public:
    WORD sHead;//固定0XFEFF 2
    DWORD nLength;//包长度 4 long=4?
    WORD sCmd;//控制命令 2
    std::vector<char> strData;//包数据
    WORD sSum;//和校验 2
    std::vector<char> packetBuffer{};
    //包头固定长度 10+data；
    //解析过程 先提取包头 符合就继续解包
};
//TODO:搭建网络框架，单例模式，智能指针

class CServersocket :public QObject
{
     Q_OBJECT // 必须有，才能使用信号/槽
public:
    static CServersocket& GetInstance() {
        static CServersocket instance;
        return instance;
    }
    bool StartListen();
    bool Send(const char* pData,int nSIze){return 1;};
    bool Send(const SPackeg& pack);
    bool GetFilePath(std::string& strPath);
    bool GetMouseEvent(MouseEvent& mouse);
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
        qDebug() << sout.c_str();
    } //测试函数
    SOCKET AcceptClient();
    int DealCommand(){
        qDebug()<<"deal command";
        if(m_client==-1)return false;
        char* buffer=new char[4096];
        memset(buffer,0,4096);
        size_t index=0;
        while (TRUE) {
            //收到包
            size_t len =recv(m_client,buffer+index,4096-index,0);
            Dump((BYTE*)(buffer + index), len);
            //qDebug()<<len;
            if(len<=0)
            {
                return -1;
            }
            //创建包对象
            index +=len;
            len=index;
            m_packet=SPackeg((BYTE*)buffer,len);
            qDebug()<<m_packet.sCmd;
            if(len>0)
            {
                memmove(buffer,buffer+len,4096-len);
                index-=len;
                qDebug()<<m_packet.sCmd;
                return m_packet.sCmd;
            }
            else
            {
                return -1;
            }
        }
    }
    void closeLink()
    {
         closesocket(m_client);
    }
    SPackeg GetPackeg()
    {
        return m_packet;
    }
private:
    SOCKET m_client = INVALID_SOCKET;
    SOCKET m_sock = INVALID_SOCKET;
    SPackeg m_packet;
    bool SetupListenSocket(int port);
    bool InitWSA(){
        WSADATA data{};
        int result =WSAStartup(MAKEWORD(1,1),&data);
        if(result!=0){
            qDebug()<< "WSAStartup failed with error: " << result;
            return FALSE;
        };
        return true;
    };

    CServersocket(){
        if(!InitWSA())
        {
            qDebug()<<"初始化网络环境错误！";
            exit(0);
        }
    };
    ~CServersocket()
    {
        qDebug()<<"析构！";
        if (m_sock != INVALID_SOCKET) closesocket(m_sock);
        if (m_client != INVALID_SOCKET) closesocket(m_client);
        WSACleanup();
    };
    //todo:禁用拷贝赋值
    CServersocket(const CServersocket&) = delete;
    CServersocket& operator=(const CServersocket&) = delete;
signals:
    void lockCommandReceived();
};

#endif // SERVERSOCKET_H

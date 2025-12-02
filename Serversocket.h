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
        sSum=0;
        if(nSize>=0)
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
        const BYTE* cursor=pData;
        size_t i=0;
        WORD temp_head;
        for (; i < nSize-sizeof(DWORD); i++) { // 循环直到找到包头
            // 使用 memcpy 安全地读取潜在的包头
            memcpy(&temp_head, pData + i, sizeof(WORD));
            if (temp_head == 0XFEFF) {
                // 包头找到，游标移动到包头后的位置
                cursor = pData + i + sizeof(WORD);
                break;
            }
        }
        //这里有个问题 我需要手动加一个字符串终止符。
        //读取包中长度信息
        memcpy(&nLength,cursor,sizeof(DWORD));
        cursor+=sizeof(DWORD);
        if (i + nLength > nSize) return;
        memcpy(&sCmd,cursor,sizeof(WORD));
        cursor+=sizeof(WORD);
        int payload_size = nLength - (sizeof(WORD) + sizeof(DWORD) + sizeof(WORD) + sizeof(WORD));
        if (payload_size < 0) { nSize = 0; return; }//验证数据包是否完整
        strData.resize(payload_size+1);
        memcpy(strData.data(), cursor, payload_size);
        strData[payload_size]='\0';
        cursor+=payload_size;
        memcpy(&sSum, cursor, sizeof(WORD));
        cursor += sizeof(WORD);

        WORD calculated_sum = 0;
        // 5. 计算校验和：
        for (size_t j = 0; j < strData.size(); j++) {
            // 使用 unsigned char 确保求和是基于 0-255 的值
            calculated_sum += (unsigned char)strData[j];
        }
        // 6. 校验结果
        if (calculated_sum != sSum) {
            qDebug() << "校验和不匹配，数据包可能已损坏！";
            return; // 校验失败
        }
        nSize = cursor - pData;
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
    int DealCommand() {
        if (m_client == -1) return false;
        char* buffer = new char[4096];
        size_t index = 0; // 缓冲区中有效数据的总长度

        while (TRUE) { // 外部循环：持续接收数据
            // 1. 接收数据：阻塞等待新数据，写入到缓冲区的末尾
            size_t bytes_to_read = 4096 - index;
            if (bytes_to_read == 0) {
                qDebug() << "缓冲区已满，无法接收更多数据。";
                break; // 缓冲满，强制退出，防止越界
            }
            size_t len = ::recv(m_client, buffer + index, bytes_to_read, 0);
            if (len <= 0) {
                // 连接断开或错误，退出函数
                delete[] buffer;
                return false;
            }
            index += len; // 更新缓冲区中总数据长度
            // 解析累积的数据
            size_t consumed_size = 0;
            while (index >= 10) {
                size_t total_data_size = index; // 传递当前总长度
                m_packet = SPackeg((BYTE*)buffer, total_data_size);
                consumed_size = total_data_size; // 获取解析器设置的消耗长度
                if (consumed_size > 0) {
                    qDebug() << "成功解包，命令 ID:" << m_packet.sCmd;
                    // 清理缓冲区：移除已消耗的部分，将剩余数据前移
                    memmove(buffer, buffer + consumed_size, index - consumed_size);
                    index -= consumed_size; // 更新总长度
                    // 立处理这个包，并退出 DealCommand
                    delete[] buffer;
                    return m_packet.sCmd;
                } else {
                    // 包不完整，或者格式错误，退出解析循环，等待更多数据
                    break;
                }
            }
        }
        delete[] buffer;
        return false; // 如果循环意外结束
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

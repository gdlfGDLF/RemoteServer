#include "Serversocket.h"
bool CServersocket::SetupListenSocket(int port = 9527) {
    // 1. 创建 Socket
    m_sock = socket(AF_INET, SOCK_STREAM, 0); // PF_INET 等同于 AF_INET，通常用 AF_INET

    if(m_sock == INVALID_SOCKET) {
        qDebug() << "Socket creation failed: " << WSAGetLastError();
        return false;
    }
    // 2. 绑定地址
    sockaddr_in serv_adr;
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = INADDR_ANY; // 监听所有网卡
    serv_adr.sin_port = htons(port);

    if(bind(m_sock, (sockaddr*)&serv_adr, sizeof(serv_adr)) == SOCKET_ERROR) {
        qDebug() << "Bind failed: " << WSAGetLastError();
        closesocket(m_sock);
        return false;
    }

    // 3. 监听
    if(listen(m_sock, SOMAXCONN) == SOCKET_ERROR) { // SOMAXCONN 是最大连接数
        qDebug() << "Listen failed: " << WSAGetLastError();
        closesocket(m_sock);
        return false;
    }

    qDebug() << "Server listening on port " << port;
    return true;
}
bool CServersocket::StartListen() {
    return SetupListenSocket(9527);
}
SOCKET CServersocket::AcceptClient() {
    sockaddr_in client_adr;
    int cli_sz = sizeof(client_adr);

    // 阻塞等待客户端连接
    SOCKET client_sock = accept(m_sock, (sockaddr*)&client_adr, &cli_sz);

    if (client_sock == INVALID_SOCKET) {
        qDebug() << "Accept failed: " << WSAGetLastError();
        return INVALID_SOCKET;
    }

    // 提示连接信息（可选）
    qDebug() << "Client connected from: " << inet_ntoa(client_adr.sin_addr)
             << ":" << ntohs(client_adr.sin_port);

    // if (m_client != INVALID_SOCKET) {
    //     closesocket(m_client); // 关闭前一个客户端连接
    // }
    m_client = client_sock;

    return m_client;
}
bool CServersocket::GetFilePath(std::string& strPath){
    if(m_packet.sCmd>=2&&m_packet.sCmd<=4)
    {
        strPath=m_packet.strData.data();
        return TRUE;
    }
    return false;
}
bool CServersocket::GetMouseEvent(MouseEvent& mouse)
{
    if(m_packet.sCmd==5)
    {
        memcpy(&mouse,m_packet.strData.data(),sizeof(MouseEvent));
        return true;
    }
    return false;
}
bool CServersocket::Send(const SPackeg& pack) {
    const std::vector<char>& buffer = pack.getPacketBuffer();

    // 1. 获取完整数据的指针
    const char* pData = buffer.data();

    // 2. 获取准确的长度
    size_t totalSize = buffer.size(); // 或者使用 pack.nLength，但 buffer.size() 更可靠

    // 3. 调用 send，并检查是否所有字节都发送成功
    int bytes_sent = ::send(m_client, pData, totalSize, 0);

    if (bytes_sent == SOCKET_ERROR) {
        // 处理错误
        qDebug() << "Send failed:" << WSAGetLastError();
        return false;
    }

    // 检查是否所有字节都发送出去了 (TCP send 不保证一次发完)
    return (size_t)bytes_sent == totalSize;
}

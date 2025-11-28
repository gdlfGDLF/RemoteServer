#ifndef SERVERLISTENER_H
#define SERVERLISTENER_H
#include "Serversocket.h"

class ServerListener : public QObject {
    Q_OBJECT
public:
public slots:
    void listenLoop();
};


#endif // SERVERLISTENER_H

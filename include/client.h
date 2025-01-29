#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <string>
#include "common.h"

class MessageQueueClient {
public:
    std::string server_domain;
    short server_port;
    short client_port;
    int role;
    int fd;

public:
    MessageQueueClient(int role, std::string server_domain, short server_port);

    int init();

    int send(void *buffer, int length, int tag);

    int recv(void **buffer, int *length, int *tag);

    void finalize() const;

};

#endif

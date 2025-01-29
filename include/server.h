#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <string>
#include <vector>
#include <set>
#include <queue>

struct Message {
    int tag;
    int length;
    void *data;
};

class MessageQueueServer {

public:

    // Socket address
    std::string domain;
    short port;

    // File descriptors
    int server_fd;
    std::set<int> producers_fds;
    std::set<int> consumers_fds;

    // Messages
    std::queue<Message> message_queue;

public:
    MessageQueueServer(std::string domain, short port);

    int init();

    void run();

    void finalize() const;

private:

    int get_buffer_size(int optname) const;

    void handle_new_connections(struct pollfd pollfd);

    void handle_producers(std::vector<struct pollfd> &fds);

    void handle_consumers(std::vector<struct pollfd> &fds);

    int send_message(Message &message, int fd);
};

#endif

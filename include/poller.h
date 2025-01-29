#ifndef POLLER_H
#define POLLER_H

#include <poll.h>
#include <vector>

class Poller {
public:
    std::vector<struct pollfd> fds;

public:
    Poller() = default;

    void add_fd(int fd, short events);

    int Poll();

    void reset();

};

#endif

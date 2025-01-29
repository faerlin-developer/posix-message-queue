
#include <poll.h>
#include <vector>
#include <cstdio>
#include "poller.h"

void Poller::add_fd(int fd, short events) {
    struct pollfd pfd = {0};
    pfd.fd = fd;
    pfd.events = events;
    fds.push_back(pfd);
}

int Poller::Poll() {
    return poll(fds.data(), fds.size(), 0);
}

void Poller::reset() {
    fds = std::vector<struct pollfd>();
}

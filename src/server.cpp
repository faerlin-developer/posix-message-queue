
#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <poll.h>
#include <iostream>
#include <iomanip>
#include "server.h"
#include "common.h"
#include "logger.h"
#include "poller.h"

MessageQueueServer::MessageQueueServer(std::string domain, short port) : domain(domain), port(port) {}

/**
 * Initialize the server.
 * (1) Create socket to listen for new client connections.
 * (2) Bind address to socket
 * (3) Listen for connections on socket
 */
int MessageQueueServer::init() {

    logInfo("starting server...\n");

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return -1;
    }

    // Enable port re-use
    int optval = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt SO_REUSEADDR");
        close(server_fd);
        return 1;
    }

    auto recv_buffer_size = get_buffer_size(SO_RCVBUF);
    auto send_buffer_size = get_buffer_size(SO_SNDBUF);
    logInfo("receiver buffer size: %d bytes\n", recv_buffer_size);
    logInfo("send buffer size: %d bytes\n", send_buffer_size);

    // Server socker address
    struct sockaddr_in serverInfo = {0};
    serverInfo.sin_family = AF_INET;
    serverInfo.sin_addr.s_addr = inet_addr(domain.c_str());
    serverInfo.sin_port = htons(port);

    // Bind address (IP address and port number) to a socket
    int err = bind(server_fd, (struct sockaddr *) &serverInfo, sizeof(serverInfo));
    if (err == -1) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    // Listen for connections on a socket
    err = listen(server_fd, 0);
    if (err == -1) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    logInfo("started server at %s:%d\n", domain.c_str(), port);

    return 0;
}

/**
 * Run the server event loop.
 */
void MessageQueueServer::run() {

    auto poller = Poller();

    auto it = consumers_fds.begin();
    while (true) {

        // Specify the file descriptors that the poller will monitor
        // (1) server_fd listens for new client connection
        // (2) consumer_fds are the file descriptors of consumers
        // (3) producer_fds are the file descriptors of producers
        poller.reset();
        poller.add_fd(server_fd, POLLIN);

        for (const auto &fd: producers_fds) {
            poller.add_fd(fd, POLLIN);
        }

        for (const auto &fd: consumers_fds) {
            poller.add_fd(fd, POLLIN);
        }

        // Poll the file descriptors
        int n_events = poller.Poll();
        if (n_events == -1) {
            perror("poll");
            return;
        }

        // Handle the file descriptor events
        if (n_events > 0) {
            auto fds = poller.fds;
            handle_new_connections(fds[0]);
            handle_producers(fds);
            handle_consumers(fds);
        }

        // Send messages to the consumers.
        // We use round-robin scheduling to select the next consumer.
        // That is, if we reach the end of the list of consumers, loop back to the beginning.
        if (it == consumers_fds.end()) {
            it = consumers_fds.begin();
        }

        while (!message_queue.empty() && it != consumers_fds.end()) {

            // The present algorithm sends one message to each consumer
            // before exiting this loop.

            auto fd = *it;
            auto message = message_queue.front();
            auto err = send_message(message, fd);
            if (err != -1) {
                logInfo("successfully sent message to fd %d\n", fd);
                message_queue.pop();
            } else {
                logError("failed to send message to fd %d. Re-send to another consumer.\n", fd);
            }

            it++;
        }
    }
}

void MessageQueueServer::finalize() const {
    close(server_fd);
}

/**
 * Return the buffer size.
 * If optname is SO_RCVBUF, then return the receive buffer size.
 * If optname is SO_SNDBUF, then return the send buffer size.
 */
int MessageQueueServer::get_buffer_size(int optname) const {

    int buffer_size;
    socklen_t optlen = sizeof(int);
    if (getsockopt(server_fd, SOL_SOCKET, optname, &buffer_size, &optlen) == -1) {
        perror("getsockopt SO_RCVBUF");
        return -1;
    }

    return buffer_size;
}

/**
 * Register new client connection.
 */
void MessageQueueServer::handle_new_connections(struct pollfd pollfd) {

    if (pollfd.revents & POLLIN) {

        // Accept new client connection
        struct sockaddr_in clientaddr = {0};
        socklen_t clientSize = sizeof(clientaddr);
        int cfd = accept(pollfd.fd, (struct sockaddr *) &clientaddr, &clientSize);

        logInfo("New connection from %s:%d with fd %d\n",
                inet_ntoa(clientaddr.sin_addr),
                ntohs(clientaddr.sin_port),
                cfd
        );

        // Expect the first message from new client to indicate its role
        int role = -1;
        auto bytes_read = read(cfd, &role, sizeof(int));
        if (bytes_read != sizeof(int)) {
            logError("failed to read role from client for fd %d\n", cfd);
            return;
        }

        // Assign role to new client
        role = ntohl(role);
        if (role & PRODUCER) {
            producers_fds.insert(cfd);
            logInfo("fd %d is a PRODUCER\n", cfd);
        }

        if (role & CONSUMER) {
            consumers_fds.insert(cfd);
            logInfo("fd %d is a CONSUMER\n", cfd);
        }
    }
}

/**
 * Receive messages from producers and store them to internal queue.
 */
void MessageQueueServer::handle_producers(std::vector<struct pollfd> &fds) {

    for (int i = 1; i < static_cast<int>(fds.size()); i++) {

        if ((fds[i].revents & POLLIN) && producers_fds.count(fds[i].fd) > 0) {

            // Read message metadata from client.
            // If the following read is empty, the producer client has closed its socket.
            // Then close connection to this producer.
            // Otherwise, proceed to receive the expected message.
            Metadata metadata;
            auto bytes_read = read(fds[i].fd, &metadata, sizeof(metadata));
            if (bytes_read == 0) {
                logInfo("closed connection to PRODUCER fd %d\n", fds[i].fd);
                producers_fds.erase(fds[i].fd);
                continue;
            }

            // Convert to host byte-order
            metadata.tag = ntohl(metadata.tag);
            metadata.length = ntohl(metadata.length);

            // Read actual message from client
            auto buffer = malloc(metadata.length);
            bytes_read = read(fds[i].fd, buffer, metadata.length);
            if (bytes_read != metadata.length) {
                logError("failed to read data from fd %d\n", fds[i].fd);
                continue;
            }

            // Store message to internal queue
            Message message;
            message.tag = metadata.tag;
            message.length = metadata.length;
            message.data = buffer;
            message_queue.push(message);

            logInfo("successfully received message from fd %d\n", fds[i].fd);
        }
    }
}

/**
 * Disconnect from consumers who have closed their file descriptor.
 */
void MessageQueueServer::handle_consumers(std::vector<struct pollfd> &fds) {

    int dummy;
    for (int i = 1; i < static_cast<int>(fds.size()); i++) {

        if ((fds[i].revents & POLLIN) && consumers_fds.count(fds[i].fd) > 0) {

            // A consumer should not be sending the server any message.
            // We expect the following read to be empty.
            // This signals the server to close the connection to this client.
            auto bytes_read = read(fds[i].fd, &dummy, sizeof(int));

            if (bytes_read == 0) {
                logInfo("closed connection to CONSUMER fd %d\n", fds[i].fd);
                consumers_fds.erase(fds[i].fd);
            } else {
                // This should not happen
                logError("not expecting a non-empty read from fd %d\n", fds[i].fd);
            }
        }
    }
}

/**
 * Send message to provided file descriptor.
 */
int MessageQueueServer::send_message(Message &message, int fd) {

    // Send message metadata
    Metadata metadata;
    metadata.tag = htonl(message.tag);
    metadata.length = htonl(message.length);
    auto bytes_written = write(fd, &metadata, sizeof(metadata));
    if (bytes_written != sizeof(metadata)) {
        logError("failed to send message metadata to fd %d\n, fd");
        return -1;
    }

    // Send actual message
    bytes_written = write(fd, message.data, message.length);
    if (bytes_written != message.length) {
        logError("failed to send message to fd %d\n, fd");
        return -1;
    }

    return 0;
}





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
#include <cstring>
#include "client.h"
#include "common.h"
#include "logger.h"

MessageQueueClient::MessageQueueClient(int role, std::string server_domain, short server_port) :
        server_domain(server_domain), server_port(server_port), client_port(-1), role(role), fd(-1) {}

/**
 * Initialize client.
 * (1) Create socket.
 * (2) Connect to socket.
 * (3) Send role (PRODUCER or CONSUMER) to server.
 */
int MessageQueueClient::init() {

    // Create socket
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }

    // Note that sockaddr and sockaddr_in are interchangeable
    struct sockaddr_in serverInfo = {0};
    serverInfo.sin_family = AF_INET;
    serverInfo.sin_addr.s_addr = inet_addr(server_domain.c_str());
    serverInfo.sin_port = htons(server_port);

    // Initiate connection on a socket
    int err = connect(fd, (struct sockaddr *) &serverInfo, sizeof(serverInfo));
    if (err == -1) {
        perror("connect");
        close(fd);
        return -1;
    }

    // Get ephemeral client port number
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    getsockname(fd, (struct sockaddr *) &client_addr, &addr_len);
    client_port = ntohs(client_addr.sin_port);

    // Send client role to server.
    int client_role = htonl(role);
    auto bytes_written = write(fd, &client_role, sizeof(int));
    if (bytes_written != sizeof(int)) {
        logError("failed to send client role to server from local port %d\n", client_port);
        return -1;
    }

    logInfo("started client with local port %d\n", client_port);

    return 0;
}

/**
 * Send payload to message queue.
 */
int MessageQueueClient::send(void *buffer, int length, int tag) {

    if (!(role & PRODUCER)) {
        logError("client must be a producer to send message\n");
        return -1;
    }

    Metadata metadata;
    metadata.tag = htonl(tag);
    metadata.length = htonl(length);
    auto bytes_written = write(fd, &metadata, sizeof(metadata));
    if (bytes_written != sizeof(metadata)) {
        logError("failed to send message metadata from local port %d\n", client_port);
        return -1;
    }

    bytes_written = write(fd, buffer, length);

    return static_cast<int>(bytes_written);
}

/**
 *
 */
int MessageQueueClient::recv(void **buffer, int *length, int *tag) {

    if (!(role & CONSUMER)) {
        logError("client must be a consumer to receive message\n");
        return -1;
    }

    // Receive message metadata
    Metadata metadata;
    auto bytes_read = read(fd, &metadata, sizeof(metadata));
    if (bytes_read != sizeof(metadata)) {
        logError("failed to read message metadata\n");
        return -1;
    }

    // Receive actual message
    metadata.tag = ntohl(metadata.tag);
    metadata.length = ntohl(metadata.length);
    *buffer = malloc(metadata.length);
    bytes_read = read(fd, *buffer, metadata.length);
    if (bytes_read != metadata.length) {
        logError("failed to read actual message\n");
        return -1;
    }

    *tag = metadata.tag;
    *length = metadata.length;

    return 0;
}


void MessageQueueClient::finalize() const {
    close(fd);
}

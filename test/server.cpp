
#include <iostream>
#include "server.h"
#include "logger.h"

int main(int argc, char *argv[]) {

    // Create server
    auto server = MessageQueueServer("127.0.0.1", 8000);
    auto err = server.init();
    if (err == -1) {
        logError("failed to initialize message queue server\n");
        return EXIT_FAILURE;
    }

    // Run message queue server (blocking)
    server.run();

    return EXIT_SUCCESS;
}

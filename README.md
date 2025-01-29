# Posix Message Queue

We implement a message queue server and client (producer or consumer) using the `POSIX` api:
- Endpoints between client and server are TCP sockets for reliability and ordered delivery guarantees.  
- The POSIX `poll` function was used to monitor multiple file descriptors.

## Sample

In `test/server.cpp`, we create and run the message queue server:

```c++
// Create and initialize server
auto server = MessageQueueServer("127.0.0.1", 8000);
server.init();

// Run message queue server (blocking)
server.run();
```

In `test/clients.cpp`, we create three clients (one producer and two consumers):
- The producer sends three separate messages to the message queue server.
- Consumer 1 retrieves the first message from message queue server.
- Consumer 2 retrieves the second message from message queue server.
- Consumer 1 retrieves the third message from message queue server.

```c++

// Create clients
auto producer = MessageQueueClient(PRODUCER, "127.0.0.1", 8000);
auto consumer1 = MessageQueueClient(CONSUMER, "127.0.0.1", 8000);
auto consumer2 = MessageQueueClient(CONSUMER, "127.0.0.1", 8000);

// Initialize clients
// Sleep in between the initialization of consumer1 and consumer2
// to ensure that consumer1 gets registered first. This will simplify testing.
producer.init();
consumer1.init();
sleep(1);
consumer2.init();

// Send messages to message queue server
auto message1 = std::vector<int>{42};
auto message2 = std::vector<int>{52, 62};
auto message3 = std::vector<int>{72, 82, 92};
producer.send(message1.data(), 1 * sizeof(int), 1);
producer.send(message2.data(), 2 * sizeof(int), 1);
producer.send(message3.data(), 3 * sizeof(int), 1);

int *buffer;
int length;
int tag;

// Receive expected messages
// Message queue server uses round-robin scheduling.
consumer1.recv((void **) &buffer, &length, &tag);
logInfo("consumer1 receives {%d}\n", buffer[0]);

consumer2.recv((void **) &buffer, &length, &tag);
logInfo("consumer2 receives {%d, %d}\n", buffer[0], buffer[1]);

consumer1.recv((void **) &buffer, &length, &tag);
logInfo("consumer1 receives {%d, %d, %d}\n", buffer[0], buffer[1], buffer[2]);

producer.finalize();
consumer1.finalize();
consumer2.finalize();
```


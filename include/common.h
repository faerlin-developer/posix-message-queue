#ifndef COMMON_H
#define COMMON_H

enum Role {
    PRODUCER = 1,
    CONSUMER = 2
};

struct Metadata {
    int tag;
    int length;
};

#endif

#pragma once

#define PLATFORM_LINUX
// #define PLATFORM_FREERTOS

#define UROS_MAX_TRANSPORTS 4

#define UROS_MAX_TOPICS 32
#define UROS_MAX_SERVICES 32
static_assert(UROS_MAX_TOPICS <= 32);
static_assert(UROS_MAX_SERVICES <= 32);

#define UROS_TOPIC_MAX_SUBS 8
#define UROS_TOPIC_MAX_PUBS 1 // larger than 1 is not supported

#define UROS_SERVICE_MAX_CLIS 8
#define UROS_SERVICE_MAX_SRVS 1 // larger than 1 is not supported

#define UROS_NODE_MAX_SUBS 8 // both sub and srv
#define UROS_NODE_MAX_PUBS 8
#define UROS_NODE_MAX_CLIS 8
static_assert(UROS_NODE_MAX_SUBS <= 24);

#define UROS_MSG_MAX_SIZE 128

#define UROS_TRANSPORT_BUF_DEPTH 4
#define UROS_TRANSPORT_WORKER_STACK_DEPTH 512
#define UROS_TRANSPORT_WORKER_PRIORITY 3 // almost lowest
#define UROS_TRANSPORT_REQ_QUEUE_SIZE (2 * UROS_MSG_MAX_SIZE)

#define UROS_VERBOSE

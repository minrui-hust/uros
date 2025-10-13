#pragma once

#define PLATFORM_LINUX
// #define PLATFORM_FREERTOS

#define UROS_MAX_TRANSPORT 4

#define UROS_MAX_TOPICS 24
#define UROS_MAX_SERVICES 24
static_assert(UROS_MAX_TOPICS <= 24);
static_assert(UROS_MAX_SERVICES <= 24);

#define UROS_TOPIC_MAX_SUBS 8
#define UROS_TOPIC_MAX_PUBS 8

#define UROS_SERVICE_MAX_CLIS 8

#define UROS_NODE_MAX_SUBS 8 // both sub and srv
#define UROS_NODE_MAX_PUBS 8
#define UROS_NODE_MAX_CLIS 8
static_assert(UROS_NODE_MAX_SUBS <= 24);

#define UROS_MSG_MAX_SIZE 128

#define UROS_TRANSPORT_BUF_DEPTH 4
#define UROS_TRANSPORT_WORKER_STACK_DEPTH 512
#define UROS_TRANSPORT_WORKER_PRIORITY 3 // almost lowest

#define UROS_VERBOSE

#define TRANSPORTS_MANAGER TransportManagerT<TransportLocal>

#ifndef _LIGHTMQTT_TYPES_H_
#define _LIGHTMQTT_TYPES_H_

#define LMQTT_TYPE_MIN 1
#define LMQTT_TYPE_CONNECT 1
#define LMQTT_TYPE_CONNACK 2
#define LMQTT_TYPE_PUBLISH 3
#define LMQTT_TYPE_PUBACK 4
#define LMQTT_TYPE_PUBREC 5
#define LMQTT_TYPE_PUBREL 6
#define LMQTT_TYPE_PUBCOMP 7
#define LMQTT_TYPE_SUBSCRIBE 8
#define LMQTT_TYPE_SUBACK 9
#define LMQTT_TYPE_UNSUBSCRIBE 10
#define LMQTT_TYPE_UNSUBACK 11
#define LMQTT_TYPE_PINGREQ 12
#define LMQTT_TYPE_PINGRESP 13
#define LMQTT_TYPE_DISCONNECT 14
#define LMQTT_TYPE_MAX 14

typedef enum {
    LMQTT_IO_STATUS_READY = 0, /* = EOF */
    LMQTT_IO_STATUS_BLOCK_CONN,
    LMQTT_IO_STATUS_BLOCK_DATA,
    LMQTT_IO_STATUS_ERROR
} lmqtt_io_status_t;

#endif

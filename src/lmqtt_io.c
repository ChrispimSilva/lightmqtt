#include <lightmqtt/io.h>
#include <string.h>

/******************************************************************************
 * misc functions (need refactoring)
 ******************************************************************************/

static lmqtt_io_result_t decode_wrapper(void *data, u8 *buf, int buf_len,
    int *bytes_read)
{
    return lmqtt_rx_buffer_decode((lmqtt_rx_buffer_t *) data, buf, buf_len,
        bytes_read);
}

static lmqtt_io_result_t encode_wrapper(void *data, u8 *buf, int buf_len,
    int *bytes_written)
{
    return lmqtt_tx_buffer_encode((lmqtt_tx_buffer_t *) data, buf, buf_len,
        bytes_written);
}

static lmqtt_io_result_t buffer_read(lmqtt_read_t reader, void *data, u8 *buf,
    int *buf_pos, int buf_len, int *cnt)
{
    lmqtt_io_result_t result;

    result = reader(data, &buf[*buf_pos], buf_len - *buf_pos, cnt);
    *buf_pos += *cnt;

    return result;
}

static lmqtt_io_result_t buffer_write(lmqtt_write_t writer, void *data, u8 *buf,
    int *buf_pos, int *cnt)
{
    lmqtt_io_result_t result;

    result = writer(data, buf, *buf_pos, cnt);
    memmove(&buf[0], &buf[*cnt], *buf_pos - *cnt);
    *buf_pos -= *cnt;

    return result;
}

static int buffer_check(lmqtt_io_result_t io_res, int *cnt,
    lmqtt_io_status_t block_status, lmqtt_io_status_t *transfer_result,
    int *failed)
{
    if (io_res == LMQTT_IO_AGAIN) {
        /* if both input and output block prefer LMQTT_IO_STATUS_BLOCK_CONN */
        if (*transfer_result != LMQTT_IO_STATUS_BLOCK_CONN)
            *transfer_result = block_status;
        return 0;
    }

    if (io_res == LMQTT_IO_ERROR || *failed) {
        *transfer_result = LMQTT_IO_STATUS_ERROR;
        *failed = 1;
        return 0;
    }

    return *cnt > 0;
}

static lmqtt_io_status_t buffer_transfer(
    lmqtt_read_t reader, void *reader_data, lmqtt_io_status_t reader_block,
    lmqtt_write_t writer, void *writer_data, lmqtt_io_status_t writer_block,
    u8 *buf, int *buf_pos, int buf_len, int *failed)
{
    int read_allowed = 1;
    int write_allowed = 1;
    lmqtt_io_status_t result = LMQTT_IO_STATUS_READY;

    if (*failed)
        return LMQTT_IO_STATUS_ERROR;

    while (read_allowed || write_allowed) {
        int cnt;

        read_allowed = read_allowed && !*failed && *buf_pos < buf_len &&
            buffer_check(
                buffer_read(reader, reader_data, buf, buf_pos, buf_len, &cnt),
                &cnt, reader_block, &result, failed);

        write_allowed = write_allowed && !*failed && *buf_pos > 0 &&
            buffer_check(
                buffer_write(writer, writer_data, buf, buf_pos, &cnt),
                &cnt, writer_block, &result, failed);
    }

    return result;
}

/*
 * TODO: somehow we should tell the user which handle she should select() on. If
 * (a) lmqtt_rx_buffer_decode() returns LMQTT_DECODE_CONTINUE (meaning a write operation
 * would block), (b) the read handle is still readable and (c) the buffer is
 * full, the user cannot select() on the read handle. Otherwise the program will
 * enter a busy loop because the read handle remains signaled, but
 * process_input() cannot consume the buffer, which would take the read handle
 * out of its signaled state. Similarly, one should not wait on the write handle
 * which some callback is writing the input message to if the input buffer is
 * empty.
 */
lmqtt_io_status_t process_input(lmqtt_client_t *client)
{
    return buffer_transfer(
        client->read, client->data, LMQTT_IO_STATUS_BLOCK_CONN,
        (lmqtt_write_t) lmqtt_rx_buffer_decode, &client->rx_state,
            LMQTT_IO_STATUS_BLOCK_DATA,
        client->read_buf, &client->read_buf_pos, sizeof(client->read_buf),
        &client->failed);
}

/*
 * TODO: review how lmqtt_tx_buffer_encode() should handle cases where some read
 * would block, cases where there's no data to encode and cases where the buffer
 * is not enough to encode the whole command.
 */
lmqtt_io_status_t process_output(lmqtt_client_t *client)
{
    lmqtt_io_status_t result = buffer_transfer(
        (lmqtt_read_t) lmqtt_tx_buffer_encode, &client->tx_state,
            LMQTT_IO_STATUS_BLOCK_DATA,
        client->write, client->data, LMQTT_IO_STATUS_BLOCK_CONN,
        client->write_buf, &client->write_buf_pos, sizeof(client->write_buf),
        &client->failed);

    /* If a disconnect was requested wait until the output buffer is flushed;
     * then fail the connection. (In the future we may need to set an error code
     * indicating the reason for the error. Meanwhile this trick will make our
     * test client call close() on the socket.) */
    if (result == LMQTT_IO_STATUS_READY && client->internal.disconnecting) {
        client->failed = 1;
        return LMQTT_IO_STATUS_ERROR;
    }

    return result;
}

/******************************************************************************
 * lmqtt_client_t PRIVATE functions
 ******************************************************************************/

lmqtt_io_status_t client_keep_alive(lmqtt_client_t *client)
{
    int cnt;
    long s, ns;

    if (client->failed)
        return LMQTT_IO_STATUS_ERROR;

    if (!lmqtt_store_get_timeout(&client->store, &cnt, &s, &ns) ||
            s != 0 || ns != 0)
        return LMQTT_IO_STATUS_READY;

    if (cnt > 0) {
        client->failed = 1;
        return LMQTT_IO_STATUS_ERROR;
    }

    client->internal.pingreq(client);
    return LMQTT_IO_STATUS_READY;
}

static void client_set_state_initial(lmqtt_client_t *client);
static void client_set_state_connecting(lmqtt_client_t *client);
static void client_set_state_connected(lmqtt_client_t *client);
static void client_set_state_disconnecting(lmqtt_client_t *client);

static int client_on_connack_fail(void *data, lmqtt_connect_t *connect)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    client->failed = 1;

    return 1;
}

static int client_on_connack(void *data, lmqtt_connect_t *connect)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    if (connect->response.return_code == LMQTT_CONNACK_RC_ACCEPTED) {
        client->store.keep_alive = connect->keep_alive;
        client_set_state_connected(client);

        if (client->on_connect)
            client->on_connect(client->on_connect_data);
    } else {
        client->failed = 1;
    }

    return 1;
}

static int client_on_suback_fail(void *data, lmqtt_subscribe_t *subscribe)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    client->failed = 1;

    return 1;
}

static int client_on_suback(void *data, lmqtt_subscribe_t *subscribe)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    if (client->on_subscribe)
        client->on_subscribe(client->on_subscribe_data);

    return 1;
}

static int client_on_unsuback_fail(void *data, lmqtt_subscribe_t *subscribe)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    client->failed = 1;

    return 1;
}

static int client_on_unsuback(void *data, lmqtt_subscribe_t *subscribe)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    if (client->on_unsubscribe)
        client->on_unsubscribe(client->on_unsubscribe_data);

    return 1;
}

static int client_on_pingresp_fail(void *data)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    client->failed = 1;

    return 1;
}

static int client_on_pingresp(void *data)
{
    return 1;
}

static int client_do_connect_fail(lmqtt_client_t *client,
    lmqtt_connect_t *connect)
{
    return 0;
}

static int client_do_connect(lmqtt_client_t *client, lmqtt_connect_t *connect)
{
    lmqtt_store_append(&client->store, LMQTT_CLASS_CONNECT, 0, connect);

    client_set_state_connecting(client);

    return 1;
}

static int client_do_subscribe_fail(lmqtt_client_t *client,
    lmqtt_subscribe_t *subscribe)
{
    return 0;
}

static int client_do_subscribe(lmqtt_client_t *client,
    lmqtt_subscribe_t *subscribe)
{
    u16 packet_id = lmqtt_store_get_id(&client->store);

    subscribe->packet_id = packet_id;
    lmqtt_store_append(&client->store, LMQTT_CLASS_SUBSCRIBE, packet_id,
        subscribe);

    return 1;
}

static int client_do_unsubscribe_fail(lmqtt_client_t *client,
    lmqtt_subscribe_t *subscribe)
{
    return 0;
}

static int client_do_unsubscribe(lmqtt_client_t *client,
    lmqtt_subscribe_t *subscribe)
{
    u16 packet_id = lmqtt_store_get_id(&client->store);

    subscribe->packet_id = packet_id;
    lmqtt_store_append(&client->store, LMQTT_CLASS_UNSUBSCRIBE, packet_id,
        subscribe);

    return 1;
}

static int client_do_pingreq_fail(lmqtt_client_t *client)
{
    return 0;
}

static int client_do_pingreq(lmqtt_client_t *client)
{
    lmqtt_store_append(&client->store, LMQTT_CLASS_PINGREQ, 0, NULL);

    return 1;
}

static int client_do_disconnect_fail(lmqtt_client_t *client)
{
    return 0;
}

static int client_do_disconnect(lmqtt_client_t *client)
{
    lmqtt_store_append(&client->store, LMQTT_CLASS_DISCONNECT, 0, NULL);

    client_set_state_disconnecting(client);
    client->internal.disconnecting = 1;

    return 1;
}

static void client_set_state_initial(lmqtt_client_t *client)
{
    client->internal.connect = client_do_connect;
    client->internal.subscribe = client_do_subscribe_fail;
    client->internal.unsubscribe = client_do_unsubscribe_fail;
    client->internal.pingreq = client_do_pingreq;
    client->internal.disconnect = client_do_disconnect_fail;
    client->internal.rx_callbacks.on_connack = client_on_connack_fail;
    client->internal.rx_callbacks.on_suback = client_on_suback_fail;
    client->internal.rx_callbacks.on_unsuback = client_on_unsuback_fail;
    client->internal.rx_callbacks.on_pingresp = client_on_pingresp_fail;
}

static void client_set_state_connecting(lmqtt_client_t *client)
{
    client->internal.connect = client_do_connect_fail;
    client->internal.rx_callbacks.on_connack = client_on_connack;
}

static void client_set_state_connected(lmqtt_client_t *client)
{
    client->internal.subscribe = client_do_subscribe;
    client->internal.unsubscribe = client_do_unsubscribe;
    client->internal.disconnect = client_do_disconnect;
    client->internal.rx_callbacks.on_connack = client_on_connack_fail;
    client->internal.rx_callbacks.on_suback = client_on_suback;
    client->internal.rx_callbacks.on_unsuback = client_on_unsuback;
    client->internal.rx_callbacks.on_pingresp = client_on_pingresp;
}

static void client_set_state_disconnecting(lmqtt_client_t *client)
{
    client->internal.subscribe = client_do_subscribe_fail;
    client->internal.unsubscribe = client_do_unsubscribe_fail;
    client->internal.pingreq = client_do_pingreq_fail;
    client->internal.disconnect = client_do_disconnect_fail;
    client->internal.rx_callbacks.on_suback = client_on_suback_fail;
    client->internal.rx_callbacks.on_unsuback = client_on_unsuback_fail;
    client->internal.rx_callbacks.on_pingresp = client_on_pingresp_fail;
}

/******************************************************************************
 * lmqtt_client_t PUBLIC functions
 ******************************************************************************/

void lmqtt_client_initialize(lmqtt_client_t *client)
{
    memset(client, 0, sizeof(*client));

    client->rx_state.callbacks = &client->internal.rx_callbacks;
    client->rx_state.callbacks_data = client;
    client->rx_state.store = &client->store;
    client->tx_state.store = &client->store;

    client_set_state_initial(client);
}

int lmqtt_client_connect(lmqtt_client_t *client, lmqtt_connect_t *connect)
{
    return client->internal.connect(client, connect);
}

int lmqtt_client_subscribe(lmqtt_client_t *client, lmqtt_subscribe_t *subscribe)
{
    return client->internal.subscribe(client, subscribe);
}

int lmqtt_client_unsubscribe(lmqtt_client_t *client,
    lmqtt_subscribe_t *subscribe)
{
    return client->internal.unsubscribe(client, subscribe);
}

int lmqtt_client_disconnect(lmqtt_client_t *client)
{
    return client->internal.disconnect(client);
}

void lmqtt_client_set_on_connect(lmqtt_client_t *client,
    lmqtt_client_on_connect_t on_connect, void *on_connect_data)
{
    client->on_connect = on_connect;
    client->on_connect_data = on_connect_data;
}

void lmqtt_client_set_on_subscribe(lmqtt_client_t *client,
    lmqtt_client_on_subscribe_t on_subscribe, void *on_subscribe_data)
{
    client->on_subscribe = on_subscribe;
    client->on_subscribe_data = on_subscribe_data;
}

void lmqtt_client_set_on_unsubscribe(lmqtt_client_t *client,
    lmqtt_client_on_unsubscribe_t on_unsubscribe, void *on_unsubscribe_data)
{
    client->on_unsubscribe = on_unsubscribe;
    client->on_unsubscribe_data = on_unsubscribe_data;
}

void lmqtt_client_set_default_timeout(lmqtt_client_t *client, long secs)
{
    client->store.timeout = secs;
}

int lmqtt_client_get_timeout(lmqtt_client_t *client, long *secs, long *nsecs)
{
    int cnt;

    return lmqtt_store_get_timeout(&client->store, &cnt, secs, nsecs);
}

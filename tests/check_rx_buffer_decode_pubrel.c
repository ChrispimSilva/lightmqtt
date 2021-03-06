#include "check_lightmqtt.h"

#define ENTRY_COUNT 16

static lmqtt_store_entry_t entries[16];
static lmqtt_rx_buffer_t state;
static lmqtt_store_t store;
static int kind;
static lmqtt_store_value_t value;

static void init_state()
{
    memset(entries, 0, sizeof(entries));
    memset(&state, 0, sizeof(state));
    memset(&store, 0, sizeof(store));
    memset(&value, 0, sizeof(value));
    state.store = &store;
    store.get_time = &test_time_get;
    store.entries = entries;
    store.capacity = ENTRY_COUNT;
    kind = 0;
}

START_TEST(should_decode_pubrel_with_unknown_id)
{
    init_state();

    state.internal.packet_id = 0x0102;
    ck_assert_int_eq(1, rx_buffer_pubrel(&state));

    ck_assert_int_eq(1, lmqtt_store_peek(&store, &kind, &value));
    ck_assert_int_eq(LMQTT_KIND_PUBCOMP, kind);
    ck_assert_int_eq(0x0102, value.packet_id);
}
END_TEST

START_TEST(should_decode_pubrel_with_existing_id)
{
    init_state();

    lmqtt_id_set_put(&state.id_set, 0x0102);

    state.internal.packet_id = 0x0102;
    ck_assert_int_eq(1, rx_buffer_pubrel(&state));

    lmqtt_store_peek(&store, &kind, &value);
    ck_assert_int_eq(LMQTT_KIND_PUBCOMP, kind);

    ck_assert_int_eq(0, lmqtt_id_set_contains(&state.id_set, 0x0102));
}
END_TEST

START_TEST(should_not_decode_pubrel_with_full_store)
{
    int i;

    init_state();

    for (i = 0; i < store.capacity; i++)
        lmqtt_store_append(&store, LMQTT_KIND_PINGREQ, NULL);

    state.internal.packet_id = 0x0102;
    ck_assert_int_eq(0, rx_buffer_pubrel(&state));
}
END_TEST

START_TCASE("Rx buffer decode pubrel")
{
    ADD_TEST(should_decode_pubrel_with_unknown_id);
    ADD_TEST(should_decode_pubrel_with_existing_id);
    ADD_TEST(should_not_decode_pubrel_with_full_store);
}
END_TCASE

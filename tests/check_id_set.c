#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

START_TEST(should_put_items)
{
    lmqtt_id_set_t id_set;

    memset(&id_set, 0, sizeof(id_set));

    ck_assert_int_eq(1, lmqtt_id_set_put(&id_set, 3));
    ck_assert_int_eq(1, lmqtt_id_set_put(&id_set, 6));

    ck_assert_uint_eq(3, id_set.items[0]);
    ck_assert_uint_eq(6, id_set.items[1]);
}
END_TEST

START_TEST(should_not_overflow_buffer)
{
    lmqtt_id_set_t id_set;
    int i;

    memset(&id_set, 0, sizeof(id_set));

    for (i = 0; i < LMQTT_ID_LIST_SIZE; i++)
        ck_assert_int_eq(1, lmqtt_id_set_put(&id_set, i));

    ck_assert_int_eq(0, lmqtt_id_set_put(&id_set, LMQTT_ID_LIST_SIZE));
}
END_TEST

START_TEST(should_not_duplicate_items)
{
    lmqtt_id_set_t id_set;

    memset(&id_set, 0, sizeof(id_set));

    ck_assert_int_eq(1, lmqtt_id_set_put(&id_set, 3));
    ck_assert_int_eq(0, lmqtt_id_set_put(&id_set, 3));

    ck_assert_int_eq(1, id_set.count);
}
END_TEST

START_TEST(should_remove_item)
{
    lmqtt_id_set_t id_set;

    memset(&id_set, 0, sizeof(id_set));

    ck_assert_int_eq(1, lmqtt_id_set_put(&id_set, 3));
    ck_assert_int_eq(1, lmqtt_id_set_put(&id_set, 6));
    ck_assert_int_eq(1, lmqtt_id_set_put(&id_set, 9));

    ck_assert_int_eq(1, lmqtt_id_set_remove(&id_set, 6));

    ck_assert_int_eq(1, lmqtt_id_set_put(&id_set, 12));

    ck_assert_int_eq(3, id_set.count);
    ck_assert_uint_eq(3, id_set.items[0]);
    ck_assert_uint_eq(9, id_set.items[1]);
    ck_assert_uint_eq(12, id_set.items[2]);
}
END_TEST

START_TEST(should_not_remove_unknown_item)
{
    lmqtt_id_set_t id_set;

    memset(&id_set, 0, sizeof(id_set));

    lmqtt_id_set_put(&id_set, 3);
    lmqtt_id_set_put(&id_set, 6);

    ck_assert_int_eq(0, lmqtt_id_set_remove(&id_set, 0));
    ck_assert_int_eq(0, lmqtt_id_set_remove(&id_set, 9));
    ck_assert_int_eq(1, lmqtt_id_set_remove(&id_set, 6));

    ck_assert_int_eq(1, id_set.count);
}
END_TEST

START_TEST(should_test_whether_set_contains_item)
{
    lmqtt_id_set_t id_set;

    memset(&id_set, 0, sizeof(id_set));

    lmqtt_id_set_put(&id_set, 3);
    lmqtt_id_set_put(&id_set, 6);

    ck_assert_int_eq(0, lmqtt_id_set_contains(&id_set, 0));
    ck_assert_int_eq(0, lmqtt_id_set_contains(&id_set, 9));
    ck_assert_int_eq(1, lmqtt_id_set_contains(&id_set, 6));

    ck_assert_int_eq(2, id_set.count);
}
END_TEST

START_TCASE("Id list")
{
    ADD_TEST(should_put_items);
    ADD_TEST(should_not_overflow_buffer);
    ADD_TEST(should_not_duplicate_items);
    ADD_TEST(should_remove_item);
    ADD_TEST(should_not_remove_unknown_item);
    ADD_TEST(should_test_whether_set_contains_item);
}
END_TCASE

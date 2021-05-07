/*
 * Copyright 2014-2021 Jetperch LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "fitterbap/collections/list.h"
#include "fitterbap.h"
#include <string.h> // memset


struct element_s {
    uint32_t value;
    struct fbp_list_s item;
};

struct state_s {
    struct fbp_list_s list;
    struct element_s elements[16];
};

struct state_s state_;

static int setup(void ** state) {
    (void) state;
    memset(&state_, 0, sizeof(state_));
    fbp_list_initialize(&state_.list);
    for (uint32_t i = 0; i < FBP_ARRAY_SIZE(state_.elements); ++i) {
        state_.elements[i].value = i;
        fbp_list_initialize(&state_.elements[i].item);
    }
    *state = &state_;
    return 0;
}


static void list_empty(void **state) {
    struct state_s * s = (struct state_s *) *state;
    assert_true(fbp_list_is_empty(&s->list));
    assert_ptr_equal(s->list.next, &s->list);
    assert_ptr_equal(s->list.prev, &s->list);
    assert_int_equal(0, fbp_list_length(&s->list));
}

static void list_entry(void **state) {
    struct state_s * s = (struct state_s *) *state;
   assert_ptr_equal(&s->elements[0], fbp_list_entry(&s->elements[0].item, struct element_s, item));
}

static void list_add_head_one(void **state) {
    struct state_s * s = (struct state_s *) *state;
    fbp_list_add_head(&s->list, &s->elements[0].item);
    assert_false(fbp_list_is_empty(&s->list));
    assert_ptr_equal(s->list.next, &s->elements[0].item);
    assert_ptr_equal(s->list.prev, &s->elements[0].item);
    assert_ptr_equal(fbp_list_peek_head(&s->list), &s->elements[0].item);
    assert_int_equal(1, fbp_list_length(&s->list));
}

static void list_add_tail_one(void **state) {
    struct state_s * s = (struct state_s *) *state;
    fbp_list_add_tail(&s->list, &s->elements[0].item);
    assert_false(fbp_list_is_empty(&s->list));
    assert_ptr_equal(s->list.next, &s->elements[0].item);
    assert_ptr_equal(s->list.prev, &s->elements[0].item);
    assert_ptr_equal(fbp_list_peek_tail(&s->list), &s->elements[0].item);
    assert_int_equal(1, fbp_list_length(&s->list));
}

static void list_add_head_multiple(void **state) {
    struct state_s * s = (struct state_s *) *state;
    fbp_list_add_head(&s->list, &s->elements[0].item);
    fbp_list_add_head(&s->list, &s->elements[1].item);
    fbp_list_add_head(&s->list, &s->elements[2].item);
    assert_ptr_equal(fbp_list_peek_head(&s->list), &s->elements[2].item);
    assert_ptr_equal(fbp_list_peek_tail(&s->list), &s->elements[0].item);
    assert_int_equal(3, fbp_list_length(&s->list));
    assert_ptr_equal(fbp_list_remove_head(&s->list), &s->elements[2].item);
    assert_ptr_equal(fbp_list_remove_head(&s->list), &s->elements[1].item);
    assert_ptr_equal(fbp_list_remove_head(&s->list), &s->elements[0].item);
    assert_null(fbp_list_remove_head(&s->list));
}

static void list_add_tail_multiple(void **state) {
    struct state_s * s = (struct state_s *) *state;
    fbp_list_add_tail(&s->list, &s->elements[0].item);
    fbp_list_add_tail(&s->list, &s->elements[1].item);
    fbp_list_add_tail(&s->list, &s->elements[2].item);
    assert_ptr_equal(fbp_list_peek_head(&s->list), &s->elements[0].item);
    assert_ptr_equal(fbp_list_peek_tail(&s->list), &s->elements[2].item);
    assert_int_equal(3, fbp_list_length(&s->list));
    assert_ptr_equal(fbp_list_remove_tail(&s->list), &s->elements[2].item);
    assert_ptr_equal(fbp_list_remove_tail(&s->list), &s->elements[1].item);
    assert_ptr_equal(fbp_list_remove_tail(&s->list), &s->elements[0].item);
    assert_null(fbp_list_remove_tail(&s->list));
}

static void list_foreach(void **state) {
    struct state_s * s = (struct state_s *) *state;
    for (int i = 0; i < 10; ++i) {
        fbp_list_add_tail(&s->list, &s->elements[i].item);
    }
    int i = 0;
    struct fbp_list_s * item;
    fbp_list_foreach(&s->list, item) {
        assert_ptr_equal(item, &s->elements[i].item);
        ++i;
    }
    assert_int_equal(10, i);
}

static void list_foreach_reverse(void **state) {
    struct state_s * s = (struct state_s *) *state;
    for (int i = 0; i < 10; ++i) {
        fbp_list_add_head(&s->list, &s->elements[i].item);
    }
    int i = 0;
    struct fbp_list_s * item;
    fbp_list_foreach_reverse(&s->list, item) {
        assert_ptr_equal(item, &s->elements[i].item);
        ++i;
    }
}

static void list_remove(void **state) {
    struct state_s * s = (struct state_s *) *state;
    fbp_list_add_tail(&s->list, &s->elements[0].item);
    fbp_list_add_tail(&s->list, &s->elements[1].item);
    fbp_list_add_tail(&s->list, &s->elements[2].item);
    assert_int_equal(3, fbp_list_length(&s->list));
    fbp_list_remove(&s->elements[1].item);
    assert_int_equal(2, fbp_list_length(&s->list));
    fbp_list_remove(&s->elements[0].item);
    assert_ptr_equal(fbp_list_peek_head(&s->list), &s->elements[2].item);
    fbp_list_remove(&s->elements[2].item);
    assert_int_equal(0, fbp_list_length(&s->list));
}

static void list_remove_and_insert_while_iterating(void **state) {
    struct state_s *s = (struct state_s *) *state;
    fbp_list_add_tail(&s->list, &s->elements[0].item);
    fbp_list_add_tail(&s->list, &s->elements[1].item);
    fbp_list_add_tail(&s->list, &s->elements[2].item);
    int i = 0;
    struct fbp_list_s * item;
    fbp_list_foreach(&s->list, item) {
        if (i == 1) {
            fbp_list_insert_before(item, &s->elements[3].item);
            fbp_list_insert_after(item, &s->elements[4].item);
            fbp_list_remove(item);
        }
        ++i;
    }
    assert_int_equal(4, fbp_list_length(&s->list));
    assert_ptr_equal(fbp_list_index(&s->list, 0), &s->elements[0].item);
    assert_ptr_equal(fbp_list_index(&s->list, 1), &s->elements[3].item);
    assert_ptr_equal(fbp_list_index(&s->list, 2), &s->elements[4].item);
    assert_ptr_equal(fbp_list_index(&s->list, 3), &s->elements[2].item);
    assert_ptr_equal(fbp_list_index(&s->list, 4), 0);
}

static void list_remove_when_not_in_list(void **state) {
    struct state_s *s = (struct state_s *) *state;
    fbp_list_remove(&s->elements[0].item);
    fbp_list_remove(&s->elements[0].item);
    fbp_list_remove_head(&s->elements[0].item);
    fbp_list_remove_tail(&s->elements[0].item);
    fbp_list_remove(&s->elements[0].item);

    // and make sure it still works
    fbp_list_add_tail(&s->list, &s->elements[0].item);
    assert_int_equal(1, fbp_list_length(&s->list));
    assert_ptr_equal(fbp_list_index(&s->list, 0), &s->elements[0].item);
}

static void list_replace_empty(void **state) {
    (void) state;
    struct fbp_list_s item1;
    fbp_list_initialize(&item1);
    struct fbp_list_s item2;
    fbp_list_initialize(&item2);
    fbp_list_replace(&item1, &item2);
    assert_int_equal(0, fbp_list_length(&item1));
    assert_int_equal(0, fbp_list_length(&item2));
}

static void list_replace(void **state) {
    struct state_s * s = (struct state_s *) *state;
    fbp_list_add_tail(&s->list, &s->elements[0].item);
    fbp_list_add_tail(&s->list, &s->elements[1].item);
    assert_int_equal(2, fbp_list_length(&s->list));
    fbp_list_replace(&s->elements[0].item, &s->elements[2].item);
    assert_int_equal(2, fbp_list_length(&s->list));
    assert_ptr_equal(&s->elements[2].item, s->list.next);
}

static void list_replace_top(void **state) {
    struct state_s * s = (struct state_s *) *state;
    struct fbp_list_s item;
    fbp_list_initialize(&item);
    fbp_list_add_tail(&s->list, &s->elements[0].item);
    fbp_list_add_tail(&s->list, &s->elements[1].item);
    fbp_list_replace(&s->list, &item);
    assert_int_equal(0, fbp_list_length(&s->list));
    assert_int_equal(2, fbp_list_length(&item));
}

static void list_index(void **state) {
    struct state_s *s = (struct state_s *) *state;
    fbp_list_add_tail(&s->list, &s->elements[0].item);
    fbp_list_add_tail(&s->list, &s->elements[1].item);
    fbp_list_add_tail(&s->list, &s->elements[2].item);
    assert_int_equal(0, fbp_list_index_of(&s->list, &s->elements[0].item));
    assert_ptr_equal(fbp_list_index(&s->list, 0), &s->elements[0].item);
    assert_int_equal(1, fbp_list_index_of(&s->list, &s->elements[1].item));
    assert_ptr_equal(fbp_list_index(&s->list, 1), &s->elements[1].item);
    assert_int_equal(2, fbp_list_index_of(&s->list, &s->elements[2].item));
    assert_ptr_equal(fbp_list_index(&s->list, 2), &s->elements[2].item);
    assert_int_equal(-1, fbp_list_index_of(&s->list, &s->elements[3].item));
    assert_ptr_equal(fbp_list_index(&s->list, 3), 0);
}

static void list_autoremove(void **state) {
    struct state_s *s = (struct state_s *) *state;
    struct fbp_list_s a;
    struct fbp_list_s b;
    fbp_list_initialize(&a);
    fbp_list_initialize(&b);
    fbp_list_add_tail(&a, &s->elements[0].item);
    fbp_list_add_tail(&a, &s->elements[1].item);
    fbp_list_add_tail(&a, &s->elements[3].item);
    assert_int_equal(3, fbp_list_length(&a));

    fbp_list_add_tail(&b, &s->elements[2].item);
    fbp_list_add_head(&b, &s->elements[1].item);
    fbp_list_add_head(&b, &s->elements[0].item);
    fbp_list_add_tail(&b, &s->elements[3].item);
    assert_int_equal(4, fbp_list_length(&b));
    assert_int_equal(0, fbp_list_length(&a));

    assert_int_equal(0, fbp_list_index_of(&b, &s->elements[0].item));
    assert_int_equal(1, fbp_list_index_of(&b, &s->elements[1].item));
    assert_int_equal(2, fbp_list_index_of(&b, &s->elements[2].item));
    assert_int_equal(3, fbp_list_index_of(&b, &s->elements[3].item));

    // remove from its own list
    fbp_list_add_tail(&b, &s->elements[2].item);
    assert_int_equal(4, fbp_list_length(&b));
}

static void list_append(void **state) {
    struct state_s *s = (struct state_s *) *state;
    struct fbp_list_s a;
    struct fbp_list_s b;
    fbp_list_initialize(&a);
    fbp_list_initialize(&b);
    fbp_list_add_tail(&a, &s->elements[0].item);
    fbp_list_add_tail(&a, &s->elements[1].item);
    fbp_list_add_tail(&b, &s->elements[2].item);
    fbp_list_add_tail(&b, &s->elements[3].item);
    fbp_list_append(&a, &b);
    assert_int_equal(4, fbp_list_length(&a));
    assert_int_equal(0, fbp_list_length(&b));
    assert_int_equal(0, fbp_list_index_of(&a, &s->elements[0].item));
    assert_int_equal(1, fbp_list_index_of(&a, &s->elements[1].item));
    assert_int_equal(2, fbp_list_index_of(&a, &s->elements[2].item));
    assert_int_equal(3, fbp_list_index_of(&a, &s->elements[3].item));
}

static void list_iterate_and_free(void **state) {
    (void) state;
    struct fbp_list_s list;
    struct fbp_list_s * item;
    struct element_s * el;
    fbp_list_initialize(&list);
    for (int i = 0; i < 3; ++i) {
        el = test_malloc(sizeof(struct element_s));
        fbp_list_initialize(&el->item);
        fbp_list_add_tail(&list, &el->item);
    }
    assert_int_equal(3, fbp_list_length(&list));
    fbp_list_foreach(&list, item) {
        el = FBP_CONTAINER_OF(item, struct element_s, item);
        test_free(el);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup(list_empty, setup),
            cmocka_unit_test_setup(list_entry, setup),
            cmocka_unit_test_setup(list_add_head_one, setup),
            cmocka_unit_test_setup(list_add_tail_one, setup),
            cmocka_unit_test_setup(list_add_head_multiple, setup),
            cmocka_unit_test_setup(list_add_tail_multiple, setup),
            cmocka_unit_test_setup(list_foreach, setup),
            cmocka_unit_test_setup(list_foreach_reverse, setup),
            cmocka_unit_test_setup(list_remove, setup),
            cmocka_unit_test_setup(list_remove_and_insert_while_iterating, setup),
            cmocka_unit_test_setup(list_remove_when_not_in_list, setup),
            cmocka_unit_test_setup(list_replace_empty, setup),
            cmocka_unit_test_setup(list_replace, setup),
            cmocka_unit_test_setup(list_replace_top, setup),
            cmocka_unit_test_setup(list_index, setup),
            cmocka_unit_test_setup(list_autoremove, setup),
            cmocka_unit_test_setup(list_append, setup),
            cmocka_unit_test_setup(list_iterate_and_free, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

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
#include "fitterbap/cdef.h"
#include "fitterbap/fsm.h"
#include "hal_test_impl.h"  // for logging

enum {
    ST_0,
    ST_A,
    ST_B,
    ST_C,
    ST_NULL = FBP_STATE_NULL,
    ST_ANY = FBP_STATE_ANY,
    ST_SKIP = FBP_STATE_SKIP,
};

enum {
    EV_A,
    EV_B,
    EV_C,
    EV_ANY = FBP_EVENT_ANY,
    EV_RESET = FBP_EVENT_RESET,
    EV_ENTER = FBP_EVENT_ENTER,
    EV_EXIT = FBP_EVENT_EXIT,
};

static const char * event_name_(struct fbp_fsm_s * self, fbp_fsm_event_t event) {
    (void) self;
    switch (event) {
        case EV_A: return "A";
        case EV_B: return "B";
        case EV_C: return "C";
        default: return 0;
    }
}

struct transition_log_s {
    fbp_fsm_state_t state;
    fbp_fsm_event_t event;
};

struct fsm_test_state_s {
    struct fbp_fsm_s fsm;
    struct transition_log_s log[128];
    int log_entries;
};

static void transition_log(struct fsm_test_state_s * self, fbp_fsm_event_t event) {
    self->log[self->log_entries].state = self->fsm.state;
    self->log[self->log_entries].event = event;
    ++self->log_entries;
}

#define H(x) (fbp_fsm_handler) x
#define H_DEF(fn_) \
    static fbp_fsm_state_t fn_(struct fsm_test_state_s * self, \
                              fbp_fsm_event_t event)

H_DEF(handle_log) {
    transition_log(self, event);
    return ST_ANY;
}

H_DEF(h_null) {
    transition_log(self, event);
    return ST_NULL;
}

H_DEF(h_skip) {
    transition_log(self, event);
    return ST_SKIP;
}

H_DEF(ST_C_) {
    transition_log(self, event);
    return ST_C;
}

H_DEF(signal_ev_c) {
    (void) event;
    fbp_fsm_event(&self->fsm, EV_C);
    return ST_ANY;
}

static const struct fbp_fsm_state_s my_states[] = {
// state,   name, on_enter,      on_exit
    {ST_0,  "0",  0,             0},
    {ST_A,  "A",  H(handle_log), H(handle_log)},
    {ST_B,  "B",  H(handle_log), H(handle_log)},
    {ST_C,  "C",  H(handle_log), H(handle_log)},
};

static const struct fbp_fsm_transition_s my_transitions[] = {
// current, next, event, handler
    {ST_A,   ST_B,   EV_A,     0},
    {ST_A,   ST_B,   EV_B,     H(ST_C_)},
    {ST_B,   ST_B,   EV_A,     H(handle_log)},
    {ST_B,   ST_B,   EV_B,     H(ST_C_)},
    {ST_C,   ST_B,   EV_ANY,   H(handle_log)},
    {ST_ANY, ST_A,   EV_RESET, H(handle_log)},
    {ST_ANY, ST_B,   EV_ANY,   H(handle_log)},
};

static const struct fbp_fsm_transition_s my_transitions_skip[] = {
// current, next, event, handler
    {ST_A,   ST_B,   EV_A,     H(h_skip)},
    {ST_A,   ST_C,   EV_A,     H(handle_log)},
    {ST_ANY, ST_A,   EV_RESET, H(handle_log)},
};

static const struct fbp_fsm_transition_s my_transitions_null[] = {
// current, next, event, handler
    {ST_A,   ST_B,   EV_A,     H(h_null)},
    {ST_ANY, ST_A,   EV_RESET, H(handle_log)},
};

static int setup(void ** state) {
    struct fsm_test_state_s * s = (struct fsm_test_state_s *) test_calloc(1, sizeof(struct fsm_test_state_s));
    s->fsm.states = my_states;
    s->fsm.name = "my_fsm";
    s->fsm.states_count = FBP_ARRAY_SIZE(my_states);
    s->fsm.transitions = my_transitions;
    s->fsm.transitions_count = FBP_ARRAY_SIZE(my_transitions);
    s->fsm.event_name_fn = event_name_;
    *state = s;
    return 0;
}

static int teardown(void ** state) {
    struct fsm_test_state_s * s = (struct fsm_test_state_s *) *state;
    test_free(s);
    return 0;
}


static void empty(void **state) {
    struct fsm_test_state_s * s = (struct fsm_test_state_s *) *state;
    s->fsm.states_count = 0;
    s->fsm.transitions_count = 0;
    expect_assert_failure(fbp_fsm_initialize(&s->fsm));
}

#define assert_log(state_, event_, x) \
    assert_int_equal(state_, x.state); \
    assert_int_equal(event_, x.event); \

static void initialization(void **state) {
    struct fsm_test_state_s * s = (struct fsm_test_state_s *) *state;
    fbp_fsm_initialize(&s->fsm);
    assert_int_equal(2, s->log_entries);
    assert_log(ST_NULL, EV_RESET, s->log[0]);
    assert_log(ST_A,    EV_ENTER, s->log[1]);
}

static void no_handler(void **state) {
    struct fsm_test_state_s * s = (struct fsm_test_state_s *) *state;
    fbp_fsm_initialize(&s->fsm);
    s->log_entries = 0;
    fbp_fsm_event(&s->fsm, EV_A);
    assert_int_equal(2, s->log_entries);
    assert_log(ST_A, EV_EXIT, s->log[0]);
    assert_log(ST_B, EV_ENTER, s->log[1]);
}

static void with_handler(void **state) {
    struct fsm_test_state_s * s = (struct fsm_test_state_s *) *state;
    fbp_fsm_initialize(&s->fsm);
    s->log_entries = 0;
    fbp_fsm_event(&s->fsm, EV_B);
    assert_int_equal(3, s->log_entries);
    assert_log(ST_A, EV_B, s->log[0]);
    assert_log(ST_A, EV_EXIT, s->log[1]);
    assert_log(ST_C, EV_ENTER, s->log[2]);
}

static void one_state_any_event(void **state) {
    struct fsm_test_state_s * s = (struct fsm_test_state_s *) *state;
    fbp_fsm_initialize(&s->fsm);
    fbp_fsm_event(&s->fsm, EV_B);
    s->log_entries = 0;
    fbp_fsm_event(&s->fsm, EV_C);
    assert_int_equal(3, s->log_entries);
    assert_log(ST_C, EV_C,    s->log[0]);
    assert_log(ST_C, EV_EXIT, s->log[1]);
    assert_log(ST_B, EV_ENTER, s->log[2]);
}

static void any_state_one_event(void **state) {
    struct fsm_test_state_s * s = (struct fsm_test_state_s *) *state;
    fbp_fsm_initialize(&s->fsm);
    s->log_entries = 0;
    fbp_fsm_event(&s->fsm, EV_RESET);
    assert_int_equal(3, s->log_entries);
    assert_log(ST_A, EV_RESET, s->log[0]);
    assert_log(ST_A, EV_EXIT,  s->log[1]);
    assert_log(ST_A, EV_ENTER, s->log[2]);
}

static void any_state_any_event(void **state) {
    struct fsm_test_state_s * s = (struct fsm_test_state_s *) *state;
    fbp_fsm_initialize(&s->fsm);
    s->log_entries = 0;
    fbp_fsm_event(&s->fsm, EV_C);
    assert_int_equal(3, s->log_entries);
    assert_log(ST_A, EV_C,    s->log[0]);
    assert_log(ST_A, EV_EXIT, s->log[1]);
    assert_log(ST_B, EV_ENTER, s->log[2]);
}

static void transition_skip(void **state) {
    struct fsm_test_state_s * s = (struct fsm_test_state_s *) *state;
    s->fsm.transitions = my_transitions_skip;
    s->fsm.transitions_count = FBP_ARRAY_SIZE(my_transitions_skip);
    fbp_fsm_initialize(&s->fsm);
    s->log_entries = 0;
    fbp_fsm_event(&s->fsm, EV_A);
    assert_int_equal(4, s->log_entries);
    assert_log(ST_A, EV_A,    s->log[0]);
    assert_log(ST_A, EV_A,    s->log[1]);
    assert_log(ST_A, EV_EXIT, s->log[2]);
    assert_log(ST_C, EV_ENTER, s->log[3]);
}

static void transition_null(void **state) {
    struct fsm_test_state_s * s = (struct fsm_test_state_s *) *state;
    s->fsm.transitions = my_transitions_null;
    s->fsm.transitions_count = FBP_ARRAY_SIZE(my_transitions_null);
    fbp_fsm_initialize(&s->fsm);
    s->log_entries = 0;
    fbp_fsm_event(&s->fsm, EV_A);
    assert_int_equal(1, s->log_entries);
    assert_log(ST_A, EV_A,    s->log[0]);
}

static void transition_unmatched(void **state) {
    struct fsm_test_state_s * s = (struct fsm_test_state_s *) *state;
    s->fsm.transitions = my_transitions_null;
    s->fsm.transitions_count = FBP_ARRAY_SIZE(my_transitions_null);
    fbp_fsm_initialize(&s->fsm);
    s->log_entries = 0;
    fbp_fsm_event(&s->fsm, EV_C);
    assert_int_equal(0, s->log_entries);
}

static const struct fbp_fsm_transition_s transitions_reentrant[] = {
// current, next, event, handler
    {ST_A,   ST_A, EV_A,     H(signal_ev_c)},
    {ST_A,   ST_C, EV_C,     0},
    {ST_ANY, ST_A, EV_RESET, H(handle_log)},
};

static void reentrant(void **state) {
    struct fsm_test_state_s * s = (struct fsm_test_state_s *) *state;
    s->fsm.transitions = transitions_reentrant;
    s->fsm.transitions_count = FBP_ARRAY_SIZE(transitions_reentrant);
    fbp_fsm_initialize(&s->fsm);
    fbp_fsm_event(&s->fsm, EV_A);
    assert_int_equal(ST_C, s->fsm.state);
}

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(empty, setup, teardown),
            cmocka_unit_test_setup_teardown(initialization, setup, teardown),
            cmocka_unit_test_setup_teardown(no_handler, setup, teardown),
            cmocka_unit_test_setup_teardown(with_handler, setup, teardown),
            cmocka_unit_test_setup_teardown(one_state_any_event, setup, teardown),
            cmocka_unit_test_setup_teardown(any_state_one_event, setup, teardown),
            cmocka_unit_test_setup_teardown(any_state_any_event, setup, teardown),
            cmocka_unit_test_setup_teardown(transition_skip, setup, teardown),
            cmocka_unit_test_setup_teardown(transition_null, setup, teardown),
            cmocka_unit_test_setup_teardown(transition_unmatched, setup, teardown),
            cmocka_unit_test_setup_teardown(reentrant, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

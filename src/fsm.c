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

#define FBP_LOG_LEVEL FBP_LOG_LEVEL_NOTICE
#include "fitterbap/common_header.h"
#include "fitterbap/fsm.h"
#include "fitterbap/dbc.h"
#include <inttypes.h>

static const char * state_name_(struct fbp_fsm_s * self, fbp_fsm_state_t state) {
    const char * name = 0;
    if ((state >= 0) && (state < self->states_count)) {
        name = self->states[state].name;
    } else {
        switch (state) {
            case FBP_STATE_ANY: name = "any"; break;
            case FBP_STATE_NULL: name = "null"; break;
            case FBP_STATE_SKIP: name = "skip"; break;
            default: break;
        }
    }
    if (!name) {
        name = "_unnamed_";
    }
    return name;
}

static const char * event_name_(struct fbp_fsm_s * self, fbp_fsm_event_t event) {
    const char * name = 0;
    if (self->event_name_fn) {
        name = self->event_name_fn(self, event);
    }
    if (!name) {
        switch (event) {
            case FBP_EVENT_ANY: name = "any"; break;
            case FBP_EVENT_NULL: name = "null"; break;
            case FBP_EVENT_RESET: name = "reset"; break;
            case FBP_EVENT_ENTER: name = "enter"; break;
            case FBP_EVENT_EXIT: name = "exit"; break;
            default: break;
        }
    }
    if (!name) {
        name = "_unnamed_";
    }
    return name;
}

static void events_push(struct fbp_fsm_s * self, fbp_fsm_event_t event) {
    uint8_t head = self->events.head;
    uint8_t next = (head + 1) & FBP_FSM_EVENT_FIFO_MASK;
    FBP_ASSERT(next != self->events.tail);  // FULL
    self->events.buffer[head] = event;
    self->events.head = next;
}

static int8_t events_not_empty(struct fbp_fsm_s * self) {
    return (self->events.head == self->events.tail) ? 0 : 1;
}

static fbp_fsm_event_t events_pop(struct fbp_fsm_s * self) {
    fbp_fsm_event_t ev = self->events.buffer[self->events.tail];
    self->events.tail = (self->events.tail + 1) & FBP_FSM_EVENT_FIFO_MASK;
    return ev;
}

void fbp_fsm_initialize(struct fbp_fsm_s * self) {
    FBP_DBC_NOT_NULL(self);
    for (int32_t idx = 0; idx < (int32_t) self->states_count; ++idx) {
        int32_t state = (int32_t) self->states[idx].state;
        if (idx != state) {
            FBP_LOG_CRITICAL("state idx %" PRId32 " has id %" PRId32,
                              idx, state);
            FBP_FATAL("invalid state machine");
        }
    }
    if (!self->name) {
        self->name = "fsm";
    }
    self->events.head = 0;
    self->events.tail = 0;
    self->reentrant = 0;
    self->state = FBP_STATE_NULL;
    fbp_fsm_event(self, FBP_EVENT_RESET);
    if (self->state < 0) {
        FBP_FATAL("initialize reset failed");
    }
}

static void transition(struct fbp_fsm_s * self, fbp_fsm_state_t next, fbp_fsm_event_t event) {
    fbp_fsm_handler exit_handler = 0;
    fbp_fsm_handler enter_handler = 0;

    if (next == FBP_STATE_NULL) {
        return; // no state change.
    }
    if ((self->state >= 0) && (self->state < self->states_count)) {
        exit_handler = self->states[self->state].on_exit;
    }

    if ((next >= 0) && (next < self->states_count)) {
        enter_handler = self->states[next].on_enter;
    }

    FBP_LOGD1("%s %s --> %s on %s", self->name,
              state_name_(self, self->state),
              state_name_(self, next),
              event_name_(self, event));

    if (exit_handler) {
        exit_handler(self, FBP_EVENT_EXIT);
    }
    self->state = next;
    if (enter_handler) {
        enter_handler(self, FBP_EVENT_ENTER);
    }
}

static void handle_event(struct fbp_fsm_s * self,
                         fbp_fsm_event_t event) {
    FBP_DBC_NOT_NULL(self);
    for (int32_t idx = 0; idx < (int32_t) self->transitions_count; ++idx) {
        struct fbp_fsm_transition_s const *t = self->transitions + idx;
        if ((t->current == self->state) || (t->current == FBP_STATE_ANY)) {
            if ((t->event == event) || (t->event == FBP_EVENT_ANY)) {
                FBP_LOGD2("%s.%s transition %" PRId32 " found: %s --> %s on %s",
                          self->name, state_name_(self, self->state), idx,
                          state_name_(self, t->current),
                          state_name_(self, t->next),
                          event_name_(self, event));
                fbp_fsm_state_t next = t->next;
                if (t->handler) {
                    fbp_fsm_state_t next2 = t->handler(self, event);
                    switch (next2) {
                        case FBP_STATE_NULL:
                            return;  // matched but stay in state
                        case FBP_STATE_ANY:
                            break;   // transition allowed
                        case FBP_STATE_SKIP:
                            continue;  // not this transition!
                        default:
                            next = next2;  // transition override
                            break;
                    }
                }
                transition(self, next, event);
                return;
            }
        }
    }
    FBP_LOGD1("%s transition not found: state=%s, event=%s",
              self->name,
              state_name_(self, self->state),
              event_name_(self, event));
}


void fbp_fsm_event(struct fbp_fsm_s * self,
                  fbp_fsm_event_t event) {
    FBP_DBC_NOT_NULL(self);
    events_push(self, event);
    if (self->reentrant) {
        return;
    }
    self->reentrant = 1;
    while (events_not_empty(self)) {
        event = events_pop(self);
        handle_event(self, event);
    }
    self->reentrant = 0;
}

void fbp_fsm_reset(struct fbp_fsm_s * self) {
    FBP_DBC_NOT_NULL(self);
    fbp_fsm_event(self, FBP_EVENT_RESET);
}

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

/**
 * @file
 *
 * @brief The FBP finite state machine base implementation.
 */

#ifndef FBP_FSM_H_
#define FBP_FSM_H_

#include "fitterbap/cmacro_inc.h"
#include <stdint.h>

/**
 * @ingroup fbp_core
 * @defgroup fbp_fsm Finite state machine
 *
 * @brief The FBP finite state machine base implementation.
 *
 * @{
 */

FBP_CPP_GUARD_START

/**
 * @brief The default event definitions common to all FSM implementations.
 */
enum fbp_event_defaults_e {
    FBP_EVENT_ANY    = -1,
    FBP_EVENT_NULL   = -2,
    FBP_EVENT_RESET  = -3,
    FBP_EVENT_ENTER  = -4,
    FBP_EVENT_EXIT   = -5
};

/**
 * @brief The default state definitions common to all FSM implementations.
 */
enum fbp_state_defaults_e {
    FBP_STATE_ANY  = -1,
    FBP_STATE_NULL = -2,
    FBP_STATE_SKIP = -3,
};

struct fbp_fsm_s;  /* forward declaration */

/** The state type. */
typedef int8_t fbp_fsm_state_t;

/** The event type. */
typedef int8_t fbp_fsm_event_t;

#define FBP_FSM_EVENT_FIFO_POW2_SIZE 3
#define FBP_FSM_EVENT_FIFO_SIZE (1 << FBP_FSM_EVENT_FIFO_POW2_SIZE)
#define FBP_FSM_EVENT_FIFO_MASK (FBP_FSM_EVENT_FIFO_SIZE - 1)

/** The event FIFO. */
struct fbp_fsm_event_fifo_s {
    uint8_t head;   ///< The head for writes.
    uint8_t tail;   ///< The tail for reads.
    fbp_fsm_event_t buffer[FBP_FSM_EVENT_FIFO_SIZE];   ///< The data buffer.
};

/**
 * @brief A handler for state machine events.
 *
 * @param self The state instance.
 * @param event The event to handle.
 * @return The target state or FBP_STATE_NULL.
 */
typedef fbp_fsm_state_t (*fbp_fsm_handler)(struct fbp_fsm_s * self,
                                         fbp_fsm_event_t event);

/**
 * @brief A handler to get the developer-meaningful event name.
 *
 * @param self The state instance.
 * @param event The event.
 * @return The event name or 0.
 */
typedef const char * (*fbp_fsm_event_name_cbk)(struct fbp_fsm_s * self,
                                              fbp_fsm_event_t event);

/**
 * @brief Define a single transition (edge) for the finite state machine.
 *
 * Transitions are searched in order for the first matching transition.
 * On a transition, the next state is initialized with the value in next.
 * If handler is defined, then the next state will be updated with its result
 * unless it returns FBP_STATE_NULL.
 */
struct fbp_fsm_transition_s {
    /**
     * @brief The current state.
     *
     * Use FBP_STATE_ANY to match any state.
     */
    fbp_fsm_state_t current;

    /**
     * @brief The next state.
     *
     * Use FBP_STATE_NULL to always use the handler's output.
     */
    fbp_fsm_state_t next;

    /**
     * @brief The event for the transition.
     *
     * Use FBP_EVENT_ANY to match any event.
     */
    fbp_fsm_event_t event;

    /**
     * @brief The handler for this transition or NULL.
     *
     * When NULL and next is NULL, do not transition.  When NULL and
     * next, always transition to next.
     *
     * When not NULL, the action depends upon the return code.
     *
     * If FBP_STATE_ANY, then transition to next.  When handler is a guard,
     * FBP_STATE_ANY indicates that the guard matched.
     *
     * If FBP_STATE_SKIP, then continue looking for another matching transition.
     * When handler is a guard, FBP_STATE_SKIP indicates that the guard check
     * failed.  The state machine engine will continue to search for another
     * transition edge.
     *
     * If FBP_STATE_NULL, then do not transition.  This case indicates that
     * this transition matches but no action should be taken.  One common
     * use is to perform logic in this transition, such as incrementing a
     * counter, without exiting and then reentering the same state.
     *
     * If any other value, then transition directly to the returned state.
     * Directly specifying a state from the handler violates the notion of a
     * nice state transition table and can lead to spaghetti code.
     * Use sparingly if at all!
     */
    fbp_fsm_handler handler;
};

/**
 * @brief Define a single state in the finite state machine.
 */
struct fbp_fsm_state_s {
    /**
     * @brief The state id.
     *
     * This state field must match the index of this entry in fbp_fsm_s.state.
     * This field is only used to validate that the list of states was
     * properly declared.
     */
    fbp_fsm_state_t state;

    /**
     * @brief The state name log messages and debugging
     */
    const char * name;

    /**
     * @brief The function called when entering the state.
     *
     * If NULL, then no function will be call on enter.  The return
     * code is ignored.
     */
    fbp_fsm_handler on_enter;

    /**
     * @brief The function called when entering the state.
     *
     * If NULL, then no function will be call on enter.  The return
     * code is ignored.  Use this on_exit feature with care since
     * it will be called AFTER the matching transition is evaluated.
     */
    fbp_fsm_handler on_exit;
#if 0
    /**
     * @brief The list of local transitions from this state.
     *
     * This field provides an optimization that allows that state machine
     * engine to immediately access the transitions for just this state.
     * The "current" field is ignored.  If no matching transition is
     * The list of global transitions in the state machine. */
    struct fbp_fsm_transition_s const * transitions;

    /** The number of entries in transitions */
    uint32_t transitions_count;
#endif
};

/**
 * @brief The finite state machine instance.
 *
 * This structure contains an abstract class definition for all state machine
 * instances.  Individual state machine instances may provide additional data
 * by allocated a structure with an fbp_fsm_s field as the first element.
 */
struct fbp_fsm_s {
    /** The state machine name for log messages and debugging */
    const char * name;
    /** The current state for the state machine. */
    fbp_fsm_state_t state;
    /** The list of states in the state machine. */
    struct fbp_fsm_state_s const * states;
    /** The number of entries in states */
    fbp_fsm_state_t states_count;
    /** The list of global transitions in the state machine. */
    struct fbp_fsm_transition_s const * transitions;
    /** The number of entries in transitions */
    uint32_t transitions_count;
    /** The function called to get the event name. */
    fbp_fsm_event_name_cbk event_name_fn;
    /** The event FIFO. */
    struct fbp_fsm_event_fifo_s events;
    /** Flag used to queue reentrant calls to fbp_fsm_event(). */
    uint8_t reentrant;
};

/**
 * @brief Initialize a state machine instance.
 *
 * @param self The state machine instance which must contain
 *      states, state_count, transitions and transition_count.
 *
 * This function will validate all state definitions and transitions.
 * If validation fails, this function will assert.  After validation,
 * this function internally generates an FBP_EVENT_RESET in FBP_STATE_NULL.
 */
FBP_API void fbp_fsm_initialize(struct fbp_fsm_s * self);

/**
 * @brief Provide an event to the state machine.
 *
 * @param self The state machine instance.
 * @param event The event to handle.
 */
FBP_API void fbp_fsm_event(struct fbp_fsm_s * self,
                         fbp_fsm_event_t event);

/**
 * @brief Provide a reset event to the state machine.
 *
 * @param self The state machine instance.
 */
FBP_API void fbp_fsm_reset(struct fbp_fsm_s * self);

FBP_CPP_GUARD_END

/** @} */

#endif /* FBP_FSM_H_ */

/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _MODULES_COMMON_H_
#define _MODULES_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Set the initial state for a module.
 *
 *  @note The macro requires that a state machine object named s_obj is defined and that a
 *	  state array named states is defined.
 *
 *  @param _state State to set as initial.
 *
 *  @return see smf_set_initial().
 */
#define STATE_SET_INITIAL(_state)	smf_set_initial(SMF_CTX(&s_obj), &states[_state])

/** @brief Set the state for a module.
 *
 *  @note The macro requires that a state machine object named s_obj is defined and that a
 *	  state array named states is defined.
 *
 *  @param _state State to set.
 *
 *  @return see smf_set_state().
 */
#define STATE_SET(_state)		smf_set_state(SMF_CTX(&s_obj), &states[_state])

/** @brief Run the state machine for a module.
 *
 *  @note The macro requires that a state machine object named s_obj is defined.
 *
 *  @return see smf_run_state().
 */
#define STATE_RUN()			smf_run_state(SMF_CTX(&s_obj))

#ifdef __cplusplus
}
#endif

#endif /* _MODULES_COMMON_H_ */

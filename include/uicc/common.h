#include <stdbool.h>
#include <stdint.h>

#pragma once

#define UICC_DATA_MAX_SHRT 256U
#define UICC_DATA_MAX_LONG 65536U
#define UICC_DATA_MAX UICC_DATA_MAX_SHRT

/**
 * All possible return codes that can get returned from the functions of this
 * library.
 */
typedef enum uicc_ret_e
{
    UICC_RET_UNKNOWN = 0,
    UICC_RET_SUCCESS =
        1, /* In principle =1, allows for use as 'if' condition. */
    UICC_RET_APDU_HDR_TOO_SHORT,
    UICC_RET_APDU_UNHANDLED,
    UICC_RET_APDU_RES_INVALID,
    UICC_RET_TPDU_HDR_TOO_SHORT,
    UICC_RET_BUFFER_TOO_SHORT,

    UICC_RET_FSM_TRANSITION_WAIT, /* Wait for I/O state change then run FSM. */
    UICC_RET_FSM_TRANSITION_NOW,  /* Without waiting, let the FSM run again. */

    UICC_RET_PPS_INVALID, /* E.g. the check byte is incorrect etc... */
    UICC_RET_PPS_FAILED,  /* Request is handled but params are not accepted */

    UICC_RET_ATR_INVALID, /* E.g. the ATR might not contain madatory fields or
                             is malformed. */
    UICC_RET_FS_FAILURE,  /* Unspecified FS crticial error. */
    UICC_RET_FS_FILE_NOT_FOUND, /* E.g. SELECT with FID was done but a file with
                            the given FID does not exist. */

    UICC_RET_DO_BERTLV_NOT_FOUND, /* E.g. tried to find a BER-TLV by tag but it
                             was not found in a given DO. */
    UICC_RET_DO_BERTLV_INVALID,   /* E.g. tried to parse a BER-TLV but it turned
                                     out to be incorrectly encoded thus invalid.
                                   */
} uicc_ret_et;

/**
 * Since many modules will need this, it is typedef'd here to avoid circular
 * includes.
 */
typedef struct uicc_s uicc_st;

/**
 * @brief Compute the elementary time unit (ETU) as described in ISO 7816-3:2006
 * p.13 sec.7.1.
 * @param etu Where the computed ETU will be written.
 * @param fi The clock rate conversion integer (Fi).
 * @param di The baud rate adjustment integer (Di).
 * @param fmax The maximum supported clock frequency (f(max)).
 */
void uicc_etu(uint32_t *const etu, uint16_t const fi, uint8_t const di,
              uint32_t const fmax);

/**
 * @brief Compute check byte for a buffer. This means the result of XOR'ing all
 * bytes together. ISO 7816-3:2006 p.18 sec.8.2.5.
 * @param buf_raw Buffer.
 * @param buf_raw_len Length of the buffer.
 * @return XOR of all bytes in the buffer.
 */
uint8_t uicc_tck(uint8_t const *const buf_raw, uint16_t const buf_raw_len);

/**
 * @brief Perform a hard reset of the UICC state.
 * @param uicc_state
 * @return Return code.
 * @note No other state is kept internally so this is sufficient as an analog to
 * the deactivation of a UICC.
 */
uicc_ret_et uicc_reset(uicc_st *const uicc_state);

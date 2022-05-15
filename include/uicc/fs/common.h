#pragma once

#include "uicc/common.h"
#include <assert.h>

/* It is typedef'd here to avoid circular includes. */
typedef struct uicc_fs_s uicc_fs_st;

#define UICC_FS_NAME_LEN_MAX 16U
#define UICC_FS_DEPTH_MAX 3U

/**
 * These values are used when a file does not have an ID and/or SID. This also
 * means that a valid ID or SID will not have these values.
 */
#define UICC_FS_ID_MISSING 0U
#define UICC_FS_SID_MISSING 0U

#define UICC_FS_ADF_AID_RID_LEN 5U
#define UICC_FS_ADF_AID_PIX_LEN 11U
#define UICC_FS_ADF_AID_LEN (UICC_FS_ADF_AID_RID_LEN + UICC_FS_ADF_AID_PIX_LEN)

typedef enum uicc_fs_item_type_e
{
    UICC_FS_ITEM_TYPE_INVALID,

    UICC_FS_ITEM_TYPE_FILE_MF,
    UICC_FS_ITEM_TYPE_FILE_ADF,
    UICC_FS_ITEM_TYPE_FILE_DF,
    UICC_FS_ITEM_TYPE_FILE_EF_TRANSPARENT,
    UICC_FS_ITEM_TYPE_FILE_EF_LINEARFIXED,
    // UICC_FS_ITEM_TYPE_FILE_EF_LINEARVARIABLE,
    UICC_FS_ITEM_TYPE_FILE_EF_CYCLIC,
    // UICC_FS_ITEM_TYPE_FILE_EF_DATO,

    UICC_FS_ITEM_TYPE_DATO_BERTLV,
    UICC_FS_ITEM_TYPE_HEX,
    UICC_FS_ITEM_TYPE_ASCII,
} uicc_fs_item_type_et;

/**
 * Life cycle status as specified in ISO 7816-4:2020 p.31 sec.7.4.10 table.15.
 */
typedef enum uicc_fs_lcs_e
{
    // UICC_FS_LCS_NINFO,     /* No info given */
    // UICC_FS_LCS_CREAT,     /* Creation */
    // UICC_FS_LCS_INIT,      /* Initialization */
    UICC_FS_LCS_OPER_ACTIV,   /* Operational + Activated */
    UICC_FS_LCS_OPER_DEACTIV, /* Operational + Deactivated */
    UICC_FS_LCS_TERM,         /* Termination */
} uicc_fs_lcs_et;

typedef enum uicc_fs_path_type_e
{
    UICC_FS_PATH_TYPE_MF, /* Relative to the MF. */
    UICC_FS_PATH_TYPE_DF, /* Relative to the current DF. */
} uicc_fs_path_type_et;

typedef uint16_t uicc_fs_id_kt;      /* ID like FID. */
typedef uint8_t uicc_fs_sid_kt;      /* Short ID like SFI. */
typedef uint8_t uicc_fs_rcrd_idx_kt; /* Record index. */

/**
 * A represenatation of a header of any item in the UICC FS.
 */
typedef struct uicc_fs_item_hdr_s
{
    uint32_t size;
    uicc_fs_lcs_et lcs;
    uicc_fs_item_type_et type;

    /* Offset from top of the tree to the header of this item. */
    uint32_t offset_trel;

    /**
     * Offset from the start of the header of the parent to this item. A 0 means
     * the item has no parent.
     */
    uint32_t offset_prel;
} uicc_fs_item_hdr_st;
typedef struct uicc_fs_item_hdr_raw_s
{
    uint32_t size;
    uint8_t lcs;
    uint8_t type;
    uint32_t offset_prel;
} __attribute__((packed)) uicc_fs_item_hdr_raw_st;

/**
 * Common header for all files (MF, EF, ADF, DF).
 */
typedef struct uicc_fs_file_hdr_s
{
    uicc_fs_item_hdr_st item;
    uicc_fs_id_kt id;
    uicc_fs_sid_kt sid;
    char name[UICC_FS_NAME_LEN_MAX + 1U]; /* +1U for null-terminator */
} uicc_fs_file_hdr_st;
typedef struct uicc_fs_file_hdr_raw_s
{
    uicc_fs_item_hdr_raw_st item;
    uicc_fs_id_kt id;
    uicc_fs_sid_kt sid;
    char name[UICC_FS_NAME_LEN_MAX + 1U]; /* +1U for null-terminator */
} __attribute__((packed)) uicc_fs_file_hdr_raw_st;

typedef struct uicc_fs_adf_hdr_s
{
    uicc_fs_file_hdr_st file;
    /**
     * This is for the Application IDentifier which is present ONLY for ADFs.
     * ETSI TS 101 220 v15.2.0.
     */
    struct
    {
        /* Registered application provider IDentifier. */
        uint8_t rid[UICC_FS_ADF_AID_RID_LEN];

        /* Proprietary application Identifier eXtension. */
        uint8_t pix[UICC_FS_ADF_AID_PIX_LEN];
    } aid;
} uicc_fs_adf_hdr_st;
typedef struct uicc_fs_adf_hdr_raw_s
{
    uicc_fs_file_hdr_raw_st file;
    struct
    {
        uint8_t rid[UICC_FS_ADF_AID_RID_LEN];
        uint8_t pix[UICC_FS_ADF_AID_PIX_LEN];
    } __attribute__((packed)) aid;
} __attribute__((packed)) uicc_fs_adf_hdr_raw_st;
static_assert(sizeof((uicc_fs_adf_hdr_raw_st){0U}.aid) == UICC_FS_ADF_AID_LEN,
              "AID has an unexpected size in the raw header struct of ADF");

/**
 * Header of a linear fixed EF.
 */
typedef struct uicc_fs_ef_linearfixed_hdr_s
{
    uicc_fs_file_hdr_st file;
    uint8_t rcrd_size;
} uicc_fs_ef_linearfixed_hdr_st;
typedef struct uicc_fs_ef_linearfixed_hdr_raw_s
{
    uicc_fs_file_hdr_raw_st file;
    uint8_t rcrd_size;
} __attribute__((packed)) uicc_fs_ef_linearfixed_hdr_raw_st;

/**
 * Header of a cyclic EF is the same as for a linear fixed EF.
 */
typedef uicc_fs_ef_linearfixed_hdr_st uicc_fs_ef_cyclic_hdr_st;
typedef uicc_fs_ef_linearfixed_hdr_raw_st uicc_fs_ef_cyclic_hdr_raw_st;

/* Describes a record of an EF. */
typedef struct uicc_fs_rcrd_s
{
    uint32_t size;
    uint32_t parent_offset_trel;
    uint32_t offset_prel_start;
    uicc_fs_rcrd_id_kt id;
    uicc_fs_rcrd_idx_kt idx;
} uicc_fs_rcrd_st;

/* Describes a transparent buffer. */
typedef struct uicc_fs_data_s
{
    uint32_t size;
    uint32_t parent_offset_trel;
    uint32_t offset_prel_start;
    uint32_t offset_prel_select;
} uicc_fs_data_st;

/* Describes a data object or a part of one. */
typedef struct uicc_fs_do_s
{
    uint32_t size;
    uint32_t parent_offset_trel;
    uint32_t offset_prel_start;
} uicc_fs_do_st;

typedef struct uicc_fs_path_s
{
    uicc_fs_path_type_et type;
    uint8_t *b;
    uint32_t len;
} uicc_fs_path_st;

/**
 * @brief Parse an item header.
 * @param item_hdr_raw Item header to parse.
 * @param item_hdr Where to store the parsed item header.
 * @return Return code.
 * @note The tree offset field of the parsed item will not be populated. The
 * offset to the parent will be parsed.
 */
uicc_ret_et uicc_fs_item_hdr_prs(
    uicc_fs_item_hdr_raw_st const *const item_hdr_raw,
    uicc_fs_item_hdr_st *const item_hdr);

/**
 * @brief Parse a file header.
 * @param file_hdr_raw File header to parse.
 * @param file_hdr Where to store the parsed file header.
 * @return Return code.
 * @note The item portion of the header will not be parsed.
 */
uicc_ret_et uicc_fs_file_hdr_prs(
    uicc_fs_file_hdr_raw_st const *const file_hdr_raw,
    uicc_fs_file_hdr_st *const file_hdr);

#include "uicc.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Used when creating a UICC FS disk. The 'start' size is the initial buffer
 * size, and if it's not large enough, it will grow by the 'resize' amount.
 */
#define DISK_SIZE_START 512U
#define DISK_SIZE_RESIZE 256U

/**
 * A function type for item type parsers i.e. parsers that parse a specific type
 * of item (encoded as a JSON object).
 */
typedef uicc_ret_et jsitem_prs_ft(cJSON const *const item_json,
                                  uint8_t *const buf, uint32_t *const buf_len);
/**
 * Declare early to avoid having to provide delcarations for all the individual
 * type parsers here.
 */
static jsitem_prs_ft *const jsitem_prs[];

/**
 * @brief Given a JSON item that contains a file (MF, ADF, DF, EF), parse its
 * file header and populate the raw header struct.
 * @param item_json
 * @param file_hdr_raw
 * @return Return code.
 */
static uicc_ret_et jsitem_prs_file_hdr(
    cJSON const *const item_json, uicc_fs_file_hdr_raw_st *const file_hdr_raw)
{
    uicc_ret_et ret = UICC_RET_ERROR;
    cJSON *const name_obj = cJSON_GetObjectItemCaseSensitive(item_json, "name");
    if (item_json != NULL && cJSON_IsObject(item_json) == true &&
        name_obj != NULL && cJSON_IsString(name_obj) == true)
    {
        char *const name_str = cJSON_GetStringValue(name_obj);
        if (name_str != NULL && strlen(name_str) <= UICC_FS_NAME_LEN_MAX)
        {
            /* Make sure unused bytes of the name are all NULL. */
            memset(file_hdr_raw->name, 0U, sizeof(file_hdr_raw->name));
            memcpy(file_hdr_raw->name, name_str, strlen(name_str));
            /**
             * Null-terminate the name. Safe for any name upto the maximum name
             * length because the buffer is +1 of that size.
             */
            file_hdr_raw->name[strlen(name_str)] = '\0';

            uicc_ret_et ret_id = UICC_RET_ERROR;
            cJSON *const id_obj =
                cJSON_GetObjectItemCaseSensitive(item_json, "id");
            if (id_obj != NULL && cJSON_IsString(id_obj) == true)
            {
                /* Has ID. */
                char *const id_str = cJSON_GetStringValue(id_obj);
                uicc_fs_id_kt id;
                if (id_str != NULL && strlen(id_str) == sizeof(id) * 2U)
                {
                    uint32_t id_len = sizeof(id);
                    if (uicc_hexstr_bytearr(id_str, sizeof(id) * 2U,
                                            (uint8_t *)&id,
                                            &id_len) == UICC_RET_SUCCESS &&
                        id_len == sizeof(id))
                    {
                        /**
                         * Safe cast because the bytes are just being swapped
                         * around.
                         */
                        file_hdr_raw->id = (uicc_fs_id_kt)((0x00FF & id) << 8) |
                                           ((0xFF00 & id) >> 8U);
                        ret_id = UICC_RET_SUCCESS;
                    }
                }
            }
            else
            {
                /* Has no ID. */
                file_hdr_raw->id = UICC_FS_ID_MISSING;
                ret_id = UICC_RET_SUCCESS;
            }

            uicc_ret_et ret_sid = UICC_RET_ERROR;
            cJSON *const sid_obj =
                cJSON_GetObjectItemCaseSensitive(item_json, "sid");
            if (sid_obj != NULL && cJSON_IsString(sid_obj) == true)
            {
                /* Has SID. */
                char *const sid_str = cJSON_GetStringValue(sid_obj);
                uicc_fs_sid_kt sid;
                if (sid_str != NULL && strlen(sid_str) == sizeof(sid) * 2U)
                {
                    uint32_t sid_len = sizeof(sid);
                    if (uicc_hexstr_bytearr(sid_str, sizeof(sid) * 2U,
                                            (uint8_t *)&sid,
                                            &sid_len) == UICC_RET_SUCCESS &&
                        sid_len == sizeof(sid))
                    {
                        file_hdr_raw->sid = sid;
                        ret_sid = UICC_RET_SUCCESS;
                    }
                }
            }
            else
            {
                /* Has no SID. */
                file_hdr_raw->sid = UICC_FS_SID_MISSING;
                ret_sid = UICC_RET_SUCCESS;
            }

            if (ret_id == UICC_RET_SUCCESS && ret_sid == UICC_RET_SUCCESS)
            {
                ret = UICC_RET_SUCCESS;
            }
        }
    }
    return ret;
}

/**
 * @brief Parse an item type string (as contained in the disk JSON file) to a
 * member for the item type enum.
 * @param type_str
 * @param type_str_len Length of the type string to not rely on 'strlen' in case
 * of a not null-terminated string.
 * @param type Where the type will be written. If none of the types match, this
 * will get the 'invalid' enum member.
 * @return Return code.
 */
static uicc_ret_et jsitem_prs_type_str(char const *const type_str,
                                       uint16_t const type_str_len,
                                       uicc_fs_item_type_et *const type)
{
    if (type_str == NULL)
    {
        return UICC_RET_ERROR;
    }

    /**
     * XXX: The following 3 arrays (and the count variable) must be kept in sync
     * when edited. This is very important!
     */
    static char const *const type_strs[] = {
        "file_mf",
        "file_adf",
        "file_df",
        "file_ef_transparent",
        "file_ef_linear-fixed",
        "file_ef_cyclic",
        "dato_ber-tlv",
        "hex",
        "ascii",
    };
    static uicc_fs_item_type_et const type_enums[] = {
        UICC_FS_ITEM_TYPE_FILE_MF,
        UICC_FS_ITEM_TYPE_FILE_ADF,
        UICC_FS_ITEM_TYPE_FILE_DF,
        UICC_FS_ITEM_TYPE_FILE_EF_TRANSPARENT,
        UICC_FS_ITEM_TYPE_FILE_EF_LINEARFIXED,
        UICC_FS_ITEM_TYPE_FILE_EF_CYCLIC,
        UICC_FS_ITEM_TYPE_DATO_BERTLV,
        UICC_FS_ITEM_TYPE_HEX,
        UICC_FS_ITEM_TYPE_ASCII,
    };
    static const uint8_t type_count = 9U;

    *type = UICC_FS_ITEM_TYPE_INVALID;
    for (uint8_t type_idx = 0U; type_idx < type_count; ++type_idx)
    {
        if (type_str_len == strlen(type_strs[type_idx]) &&
            memcmp(type_str, type_strs[type_idx],
                   strlen(type_strs[type_idx])) == 0)
        {
            *type = type_enums[type_idx];
            break;
        }
    }
    return UICC_RET_SUCCESS;
}

/**
 * @brief Parse an item in the JSON disk and write the parsed representation (in
 * UICC FS format) into the given buffer.
 * @param item Item to parse.
 * @param buf Where to write the parsed item.
 * @param buf_len Must hold the maximum size of the buffer. The size of the
 * parsed representation will be written here on success.
 * @return Return code.
 */
static jsitem_prs_ft jsitem_prs_demux;
static uicc_ret_et jsitem_prs_demux(cJSON const *const item_json,
                                    uint8_t *const buf, uint32_t *const buf_len)
{
    uicc_ret_et ret = UICC_RET_ERROR;
    cJSON *const type_obj = cJSON_GetObjectItemCaseSensitive(item_json, "type");
    if (item_json != NULL && cJSON_IsObject(item_json) == true &&
        type_obj != NULL && cJSON_IsString(type_obj) == true)
    {
        char *const type_str = cJSON_GetStringValue(type_obj);
        if (type_str != NULL)
        {
            uicc_fs_item_type_et type;
            ret = jsitem_prs_type_str(type_str, (uint16_t)strlen(type_str),
                                      &type);
            if (ret == UICC_RET_SUCCESS && type != UICC_FS_ITEM_TYPE_INVALID)
            {
                ret = jsitem_prs[type](item_json, buf, buf_len);
                /* Basically returns the return code of prs. */
            }
        }
    }
    return ret;
}

/**
 * @brief Parses the 'contents' attribute of folder items (MF, DF, ADF). This is
 * exactly the same for all folders so this is just implemented here once to
 * avoid duplicating code.
 * @param item_json The whole folder item (not the 'contents' attribute).
 * @param buf
 * @param buf_len
 * @return Return code.
 */
static jsitem_prs_ft jsitem_prs_file_folder;
static uicc_ret_et jsitem_prs_file_folder(cJSON const *const item_json,
                                          uint8_t *const buf,
                                          uint32_t *const buf_len)
{
    uicc_ret_et ret = UICC_RET_ERROR;
    cJSON *const contents_obj =
        cJSON_GetObjectItemCaseSensitive(item_json, "contents");
    if (item_json != NULL && cJSON_IsObject(item_json) == true &&
        contents_obj != NULL && cJSON_IsArray(contents_obj) == true)
    {
        cJSON *item;
        uint32_t items_len = 0U; /* Parsed length. */
        uicc_ret_et ret_item;
        cJSON_ArrayForEach(item, contents_obj)
        {
            uint32_t item_size = *buf_len - items_len;
            ret_item = jsitem_prs_demux(item, &buf[items_len], &item_size);
            if (ret_item != UICC_RET_SUCCESS)
            {
                break;
            }
            /* Increase total items length by size of parsed item. */
            items_len += item_size;
        }

        if (ret_item == UICC_RET_SUCCESS && *buf_len >= items_len)
        {
            *buf_len = items_len;
            ret = UICC_RET_SUCCESS;
        }
        else
        {
            /**
             * Buffer is too short but the item return is not indicating this.
             */
            ret = ret_item;
            if (*buf_len < items_len && ret_item != UICC_RET_BUFFER_TOO_SHORT)
            {
                /* Unexpected so can't recover. */
                ret = UICC_RET_ERROR;
            }
        }
    }
    return ret;
}

static uicc_ret_et jsitem_prs_file_mf(cJSON const *const item_json,
                                      uint8_t *const buf,
                                      uint32_t *const buf_len)
{
    uicc_fs_file_hdr_raw_st *hdr_raw;
    uicc_ret_et ret =
        jsitem_prs[UICC_FS_ITEM_TYPE_FILE_DF](item_json, buf, buf_len);
    if (ret == UICC_RET_SUCCESS)
    {
        if (*buf_len >= sizeof(*hdr_raw))
        {
            /* Safe cast because size of buffer is checked. */
            hdr_raw = (uicc_fs_file_hdr_raw_st *)buf;
            hdr_raw->item.type = UICC_FS_ITEM_TYPE_FILE_MF;
        }
        else
        {
            ret = UICC_RET_BUFFER_TOO_SHORT;
        }
    }
    return ret;
}
static uicc_ret_et jsitem_prs_file_adf(cJSON const *const item_json,
                                       uint8_t *const buf,
                                       uint32_t *const buf_len)
{
    uicc_fs_file_hdr_raw_st *hdr_raw;
    uicc_ret_et ret =
        jsitem_prs[UICC_FS_ITEM_TYPE_FILE_DF](item_json, buf, buf_len);
    if (ret == UICC_RET_SUCCESS)
    {
        if (*buf_len > sizeof(*hdr_raw))
        {
            /* Safe cast because size of buffer is checked. */
            hdr_raw = (uicc_fs_file_hdr_raw_st *)buf;
            hdr_raw->item.type = UICC_FS_ITEM_TYPE_FILE_ADF;
        }
        else
        {
            ret = UICC_RET_BUFFER_TOO_SHORT;
        }
    }
    return ret;
}
static uicc_ret_et jsitem_prs_file_df(cJSON const *const item_json,
                                      uint8_t *const buf,
                                      uint32_t *const buf_len)
{
    uicc_ret_et ret = UICC_RET_ERROR;
    uicc_fs_file_hdr_raw_st hdr_raw;
    if (item_json != NULL && cJSON_IsObject(item_json) == true)
    {
        ret = jsitem_prs_file_hdr(item_json, &hdr_raw);
        if (ret == UICC_RET_SUCCESS)
        {
            if (*buf_len >= sizeof(hdr_raw))
            {
                /* Safe because buffer length is greater than header length. */
                uint32_t items_len = (uint32_t)(*buf_len - sizeof(hdr_raw));
                ret = jsitem_prs_file_folder(item_json, &buf[sizeof(hdr_raw)],
                                             &items_len);
                if (ret == UICC_RET_SUCCESS)
                {
                    if (*buf_len >= sizeof(hdr_raw) + items_len)
                    {
                        hdr_raw.item.lcs = 0U;
                        hdr_raw.item.type = UICC_FS_ITEM_TYPE_FILE_DF;
                        /**
                         * Safe cast thanks to the size check that ensures no
                         * overflow will happen and that the buffer can hold the
                         * parsed item.
                         */
                        hdr_raw.item.size =
                            (uint32_t)(sizeof(hdr_raw) + items_len);
                        memcpy(buf, &hdr_raw, sizeof(hdr_raw));
                        /* Safe cast due to value check of buffer length. */
                        *buf_len = hdr_raw.item.size;
                        ret = UICC_RET_SUCCESS;
                    }
                    else
                    {
                        ret = UICC_RET_BUFFER_TOO_SHORT;
                    }
                }
            }
            else
            {
                ret = UICC_RET_BUFFER_TOO_SHORT;
            }
        }
    }
    return ret;
}
static uicc_ret_et jsitem_prs_file_ef_transparent(cJSON const *const item_json,
                                                  uint8_t *const buf,
                                                  uint32_t *const buf_len)
{
    uicc_ret_et ret = UICC_RET_ERROR;
    uicc_fs_file_hdr_raw_st hdr_raw;
    if (item_json != NULL && cJSON_IsObject(item_json) == true)
    {
        ret = jsitem_prs_file_hdr(item_json, &hdr_raw);
        if (ret == UICC_RET_SUCCESS)
        {
            if (*buf_len >= sizeof(hdr_raw))
            {
                memcpy(buf, &hdr_raw, sizeof(hdr_raw));
                /* Safe cast due to check of buffer length to header size. */
                uint32_t contents_len = (uint32_t)(*buf_len - sizeof(hdr_raw));
                uicc_ret_et ret_data = UICC_RET_ERROR;
                cJSON *const contents_obj =
                    cJSON_GetObjectItemCaseSensitive(item_json, "contents");
                if (contents_obj != NULL &&
                    cJSON_IsObject(contents_obj) == true)
                {
                    /**
                     * In theory, this call to demux allows the contents of a
                     * transparent file to be of any item type but these items
                     * will become a byte array and will be interpreted as one
                     * by the FS anyways.
                     */
                    ret_data = jsitem_prs_demux(
                        contents_obj, &buf[sizeof(hdr_raw)], &contents_len);
                    if (ret_data == UICC_RET_SUCCESS)
                    {
                        if (*buf_len >= sizeof(hdr_raw) + contents_len)
                        {
                            ret_data = UICC_RET_SUCCESS;
                        }
                        else
                        {
                            ret_data = UICC_RET_BUFFER_TOO_SHORT;
                        }
                    }
                }
                else if (contents_obj != NULL &&
                         cJSON_IsNull(contents_obj) == true)
                {
                    contents_len = 0U;
                    ret_data = UICC_RET_SUCCESS;
                }

                if (ret_data == UICC_RET_SUCCESS)
                {
                    hdr_raw.item.type = UICC_FS_ITEM_TYPE_FILE_EF_TRANSPARENT;
                    hdr_raw.item.lcs = 0U;
                    /**
                     * Safe cast because both values will be positive. Not sure
                     * about overflow of uint32_t.
                     */
                    hdr_raw.item.size =
                        (uint32_t)(sizeof(hdr_raw) + contents_len);
                    memcpy(buf, &hdr_raw, sizeof(hdr_raw));
                    *buf_len = (uint32_t)(contents_len + sizeof(hdr_raw));
                }
                ret = ret_data;
            }
            else
            {
                ret = UICC_RET_BUFFER_TOO_SHORT;
            }
        }
    }
    return ret;
}
static uicc_ret_et jsitem_prs_file_ef_linearfixed(cJSON const *const item_json,
                                                  uint8_t *const buf,
                                                  uint32_t *const buf_len)
{
    uicc_ret_et ret = UICC_RET_ERROR;
    uicc_fs_ef_linearfixed_hdr_raw_st hdr_raw;
    if (item_json != NULL && cJSON_IsObject(item_json) == true)
    {
        ret = jsitem_prs_file_hdr(item_json, &hdr_raw.file);
        if (ret == UICC_RET_SUCCESS)
        {
            if (*buf_len >= sizeof(hdr_raw))
            {
                cJSON *const rcrd_size_obj =
                    cJSON_GetObjectItemCaseSensitive(item_json, "rcrd_size");
                if (rcrd_size_obj != NULL &&
                    cJSON_IsNumber(rcrd_size_obj) == true)
                {
                    /**
                     * Forcing this cast because the number in the JSON should
                     * have been a natural number.
                     */
                    uint8_t const rcrd_size =
                        (uint8_t)cJSON_GetNumberValue(rcrd_size_obj);
                    hdr_raw.rcrd_size = rcrd_size;

                    cJSON *const contents_arr =
                        cJSON_GetObjectItemCaseSensitive(item_json, "contents");
                    if (contents_arr != NULL &&
                        cJSON_IsArray(contents_arr) == true)
                    {
                        cJSON *item;
                        uint32_t contents_len = 0U; /* Parsed length. */
                        uicc_ret_et ret_item;
                        cJSON_ArrayForEach(item, contents_arr)
                        {
                            /* Safe cast due to size checks. */
                            uint32_t item_size =
                                (uint32_t)(*buf_len - sizeof(hdr_raw) -
                                           contents_len);
                            /**
                             * By default, unused space must be filled with
                             * 0xFF.
                             */
                            memset(&buf[sizeof(hdr_raw) + contents_len], 0xFF,
                                   hdr_raw.rcrd_size);
                            ret_item = jsitem_prs_demux(
                                item, &buf[sizeof(hdr_raw) + contents_len],
                                &item_size);
                            if (ret_item != UICC_RET_SUCCESS ||
                                item_size > hdr_raw.rcrd_size)
                            {
                                break;
                            }

                            /* Every item must have the same length. */
                            contents_len += hdr_raw.rcrd_size;
                        }
                        /**
                         * Enusre the buffer is not overflown with the
                         * parsed items.
                         */
                        if (*buf_len < sizeof(hdr_raw) + contents_len)
                        {
                            ret_item = UICC_RET_BUFFER_TOO_SHORT;
                        }

                        /**
                         * The buffer size was already checked and the items +
                         * header will fit inside it.
                         */
                        if (ret_item == UICC_RET_SUCCESS)
                        {
                            hdr_raw.file.item.type =
                                UICC_FS_ITEM_TYPE_FILE_EF_LINEARFIXED;
                            hdr_raw.file.item.lcs = 0U;
                            /* Safe cast due to check on buffer length. */
                            hdr_raw.file.item.size =
                                (uint32_t)(sizeof(hdr_raw) + contents_len);
                            memcpy(buf, &hdr_raw, sizeof(hdr_raw));
                            *buf_len = hdr_raw.file.item.size;
                        }
                        ret = ret_item;
                    }
                }
            }
            else
            {
                ret = UICC_RET_BUFFER_TOO_SHORT;
            }
        }
    }
    return ret;
}
static uicc_ret_et jsitem_prs_file_ef_cyclic(cJSON const *const item_json,
                                             uint8_t *const buf,
                                             uint32_t *const buf_len)
{
    return jsitem_prs[UICC_FS_ITEM_TYPE_FILE_EF_LINEARFIXED](item_json, buf,
                                                             buf_len);
}
static uicc_ret_et jsitem_prs_item_dato_bertlv(cJSON const *const item_json,
                                               uint8_t *const buf,
                                               uint32_t *const buf_len)
{
    /**
     * TODO: Implement this.
     */
    *buf_len = 0;
    return UICC_RET_SUCCESS;
}
static uicc_ret_et jsitem_prs_item_hex(cJSON const *const item_json,
                                       uint8_t *const buf,
                                       uint32_t *const buf_len)
{
    uicc_ret_et ret = UICC_RET_ERROR;
    cJSON *const contents_obj =
        cJSON_GetObjectItemCaseSensitive(item_json, "contents");
    if (item_json != NULL && cJSON_IsObject(item_json) == true &&
        contents_obj != NULL && cJSON_IsString(contents_obj) == true)
    {
        char *const contents_str = cJSON_GetStringValue(contents_obj);
        if (contents_str != NULL)
        {
            uint64_t const hexstr_len = strlen(contents_str);
            if (hexstr_len <= UINT32_MAX)
            {
                uint32_t bytearr_len = *buf_len;
                /* Safe cast due to the boundary check. */
                ret = uicc_hexstr_bytearr(contents_str,
                                          (uint32_t)strlen(contents_str), buf,
                                          &bytearr_len);
                if (ret == UICC_RET_SUCCESS)
                {
                    if (*buf_len >= bytearr_len)
                    {
                        *buf_len = bytearr_len;
                    }
                    else
                    {
                        ret = UICC_RET_BUFFER_TOO_SHORT;
                    }
                }
            }
        }
    }
    return ret;
}
static uicc_ret_et jsitem_prs_item_ascii(cJSON const *const item_json,
                                         uint8_t *const buf,
                                         uint32_t *const buf_len)
{
    uicc_ret_et ret = UICC_RET_ERROR;
    cJSON *const contents_obj =
        cJSON_GetObjectItemCaseSensitive(item_json, "contents");
    if (item_json != NULL && cJSON_IsObject(item_json) == true &&
        contents_obj != NULL && cJSON_IsString(contents_obj) == true)
    {
        char *const contents_str = cJSON_GetStringValue(contents_obj);
        if (contents_str != NULL)
        {
            uint64_t const ascii_len = strlen(contents_str);
            if (ascii_len <= UINT32_MAX)
            {
                if (*buf_len >= ascii_len)
                {
                    memcpy(buf, contents_str, ascii_len);
                    *buf_len = (uint32_t)
                        ascii_len; /* Safe cast due to the boundary check. */
                    ret = UICC_RET_SUCCESS;
                }
                else
                {
                    ret = UICC_RET_BUFFER_TOO_SHORT;
                }
            }
        }
    }
    return ret;
}

/**
 * Provide a convenient lookup table for parsers for each item type.
 */
static jsitem_prs_ft *const jsitem_prs[] = {
    [UICC_FS_ITEM_TYPE_FILE_MF] = jsitem_prs_file_mf,
    [UICC_FS_ITEM_TYPE_FILE_ADF] = jsitem_prs_file_adf,
    [UICC_FS_ITEM_TYPE_FILE_DF] = jsitem_prs_file_df,
    [UICC_FS_ITEM_TYPE_FILE_EF_TRANSPARENT] = jsitem_prs_file_ef_transparent,
    [UICC_FS_ITEM_TYPE_FILE_EF_LINEARFIXED] = jsitem_prs_file_ef_linearfixed,
    [UICC_FS_ITEM_TYPE_FILE_EF_CYCLIC] = jsitem_prs_file_ef_cyclic,
    [UICC_FS_ITEM_TYPE_DATO_BERTLV] = jsitem_prs_item_dato_bertlv,
    [UICC_FS_ITEM_TYPE_HEX] = jsitem_prs_item_hex,
    [UICC_FS_ITEM_TYPE_ASCII] = jsitem_prs_item_ascii,
};

/**
 * @brief Parse the JSON of a disk into an in-memory representation (UICC FS
 * format).
 * @param disk Disk structure to populate.
 * @param disk_json Disk in JSON format.
 * @return Return code.
 */
static uicc_ret_et disk_json_prs(uicc_fs_disk_st *const disk,
                                 cJSON const *const disk_json)
{
    uicc_ret_et ret = UICC_RET_SUCCESS;
    if (disk->root != NULL)
    {
        /* Old disk must be unloaded first. */
        return UICC_RET_ERROR;
    }

    uint8_t tree_count = 0U;
    cJSON *const disk_obj = cJSON_GetObjectItemCaseSensitive(disk_json, "disk");
    if (disk_obj != NULL && cJSON_IsArray(disk_obj) == true)
    {
        cJSON *tree_obj;
        uicc_fs_tree_st *tree = NULL;
        cJSON_ArrayForEach(tree_obj, disk_obj)
        {
            /**
             * Check if this is the first item in the root. If so, add a
             * reference to the forest of trees (linked list) to the disk root.
             */
            if (tree == NULL)
            {
                tree = malloc(sizeof(*tree));
                if (tree == NULL)
                {
                    /* Nothing should have been allocated before. */
                    ret = UICC_RET_ERROR;
                    break;
                }
                disk->root = tree;
            }
            else
            {
                tree->next = malloc(sizeof(*tree));
                if (tree->next == NULL)
                {
                    ret = UICC_RET_ERROR;
                    break;
                }
                tree = tree->next;
            }

            memset(tree, 0U, sizeof(*tree));
            tree->buf = malloc(DISK_SIZE_START);
            if (tree->buf == NULL)
            {
                ret = UICC_RET_ERROR;
                break;
            }
            tree->size = DISK_SIZE_START;
            tree->len = 0U;

            uint32_t item_size;
            do
            {
                item_size = tree->size - tree->len;
                ret = jsitem_prs_demux(tree_obj, &tree->buf[tree->len],
                                       &item_size);
                if (ret == UICC_RET_BUFFER_TOO_SHORT)
                {
                    uint8_t *const buf_new =
                        realloc(tree->buf, tree->size + DISK_SIZE_RESIZE);
                    if (buf_new != NULL)
                    {
                        uint64_t const tree_buf_size_new =
                            tree->size + DISK_SIZE_RESIZE;
                        if (tree_buf_size_new > UINT32_MAX)
                        {
                            /**
                             * Tree buffer size limit has been reached.
                             */
                            ret = UICC_RET_ERROR;
                            /**
                             * No break because we still have to update the tree
                             * so it gets properly freed later.
                             */
                        }
                        tree->buf = buf_new;
                        /**
                         * Safe cast due to the bound check against uint32 max.
                         */
                        tree->size = (uint32_t)tree_buf_size_new;
                    }
                    else
                    {
                        ret = UICC_RET_ERROR;
                        break;
                    }
                }
            } while (ret == UICC_RET_BUFFER_TOO_SHORT);
            if (ret != UICC_RET_SUCCESS)
            {
                break;
            }

            /* Make sure length won't overflow after adding item size to it. */
            if (item_size > UINT32_MAX)
            {
                ret = UICC_RET_ERROR;
                break;
            }
            /**
             * Increase length by size of parsed item. Safe cast due to check to
             * see if it overflows the type.
             */
            tree->len = item_size;

            /**
             * Unsafe case which relies on there being fewer than 256 trees in
             * the root.
             */
            tree_count = (uint8_t)(tree_count + 1U);

            ret = uicc_fs_lutsid_rebuild(disk, tree);
            if (ret != UICC_RET_SUCCESS)
            {
                /**
                 * No need to clean up SID LUT since this will be done when
                 * whole root get emptied due to this error.
                 */
                break;
            }
        }

        /* Make sure the forest has been created successfully. */
        if (ret != UICC_RET_SUCCESS)
        {
            uicc_fs_disk_root_empty(disk);
            memset(disk, 0U, sizeof(*disk));
            ret = UICC_RET_ERROR;
        }
        ret = uicc_fs_lutid_rebuild(disk);
        if (ret != UICC_RET_SUCCESS)
        {
            uicc_fs_disk_lutid_empty(disk);
        }
    }
    else
    {
        ret = UICC_RET_ERROR;
    }
    return ret;
}

uicc_ret_et uicc_fsjson_disk_create(uicc_fs_disk_st *const disk,
                                    char const *const disk_json_path)
{
    uicc_ret_et ret = UICC_RET_ERROR;
    FILE *f = fopen(disk_json_path, "r");
    if (f != NULL)
    {
        /* Seek to end. */
        if (fseek(f, 0, SEEK_END) == 0)
        {
            /* Get current position of read pointer to get file size. */
            int64_t f_len_tmp = ftell(f);
            if (f_len_tmp >= 0 && f_len_tmp <= UINT32_MAX)
            {
                if (fseek(f, 0, SEEK_SET) == 0)
                {
                    /* Safe cast due to the size checks before. */
                    uint32_t const disk_json_raw_len = (uint32_t)f_len_tmp;
                    char *const disk_json_raw = malloc(disk_json_raw_len);
                    if (disk_json_raw != NULL)
                    {
                        if (fread(disk_json_raw, 1U, disk_json_raw_len, f) ==
                            disk_json_raw_len)
                        {
                            cJSON *const disk_json = cJSON_ParseWithLength(
                                disk_json_raw, disk_json_raw_len);
                            if (disk_json != NULL)
                            {
                                ret = disk_json_prs(disk, disk_json);
                                cJSON_Delete(disk_json);
                            }
                        }
                    }
                    free(disk_json_raw);
                }
            }
        }
        if (fclose(f) != 0)
        {
            ret = UICC_RET_ERROR;
        }
    }
    return ret;
}
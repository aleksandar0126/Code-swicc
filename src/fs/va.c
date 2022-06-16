#include "swicc/fs/common.h"
#include <string.h>
#include <swicc/swicc.h>

/**
 * @brief Helper for performing file selection according to the standard. Rules
 * for modifying the VA are described in ISO 7816-4:2020 p.22 sec.7.2.2.
 * @param fs
 * @param tree This tree must contain the file.
 * @param file File to select.
 * @return Return code.
 */
static swicc_ret_et va_select_file(swicc_fs_st *const fs,
                                   swicc_disk_tree_st *const tree,
                                   swicc_fs_file_st const file)
{
    swicc_fs_file_st file_root;
    swicc_ret_et ret = swicc_disk_tree_file_root(tree, &file_root);
    if (ret == SWICC_RET_SUCCESS)
    {
        swicc_fs_file_st file_parent;
        ret = swicc_disk_tree_file_parent(tree, &file, &file_parent);
        if (ret == SWICC_RET_SUCCESS)
        {
            switch (file.hdr_item.type)
            {
            case SWICC_FS_ITEM_TYPE_FILE_MF:
            case SWICC_FS_ITEM_TYPE_FILE_ADF:
                memset(&fs->va, 0U, sizeof(fs->va));
                fs->va.cur_tree = tree;
                fs->va.cur_adf = file;
                fs->va.cur_df = file;
                fs->va.cur_file = file;
                break;
            case SWICC_FS_ITEM_TYPE_FILE_DF:
                memset(&fs->va, 0U, sizeof(fs->va));
                fs->va.cur_tree = tree;
                fs->va.cur_adf = file_root;
                fs->va.cur_df = file;
                fs->va.cur_file = file;
                break;
            case SWICC_FS_ITEM_TYPE_FILE_EF_TRANSPARENT:
            case SWICC_FS_ITEM_TYPE_FILE_EF_LINEARFIXED:
            case SWICC_FS_ITEM_TYPE_FILE_EF_CYCLIC:
                /**
                 * @warning ISO 7816-4:2020 pg.23 sec.7.2.2 states that "When EF
                 * selection occurs as a side-effect of a C-RP using referencing
                 * by short EF identifier, curEF may change, while curDF does
                 * not change" but in this implementation, current DF always
                 * changes even for selections using SID.
                 */
                memset(&fs->va, 0U, sizeof(fs->va));
                fs->va.cur_tree = tree;
                fs->va.cur_adf = file_root;
                fs->va.cur_df = file_parent;
                fs->va.cur_ef = file;
                fs->va.cur_file = file;
                break;
            default:
                return SWICC_RET_ERROR;
            }
        }
    }
    return ret;
}

swicc_ret_et swicc_va_reset(swicc_fs_st *const fs)
{
    memset(&fs->va, 0U, sizeof(fs->va));
    swicc_disk_tree_iter_st tree_iter;
    swicc_ret_et ret = swicc_disk_tree_iter(&fs->disk, &tree_iter);
    if (ret == SWICC_RET_SUCCESS)
    {
        ret = swicc_va_select_file_id(fs, 0x3F00);
        if (ret == SWICC_RET_SUCCESS)
        {
            return ret;
        }
    }
    return ret;
}

swicc_ret_et swicc_va_select_adf(swicc_fs_st *const fs,
                                 uint8_t const *const aid,
                                 uint32_t const pix_len)
{
    swicc_disk_tree_iter_st tree_iter;
    swicc_ret_et ret = swicc_disk_tree_iter(&fs->disk, &tree_iter);
    if (ret == SWICC_RET_SUCCESS)
    {
        swicc_disk_tree_st *tree;
        do
        {
            ret = swicc_disk_tree_iter_next(&tree_iter, &tree);
            if (ret == SWICC_RET_SUCCESS)
            {
                swicc_fs_file_st file_root;
                ret = swicc_disk_tree_file_root(tree, &file_root);
                if (ret == SWICC_RET_SUCCESS)
                {
                    if (file_root.hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_ADF)
                    {
                        if (memcmp(file_root.hdr_spec.adf.aid.rid, aid,
                                   SWICC_FS_ADF_AID_RID_LEN) == 0U &&
                            memcmp(file_root.hdr_spec.adf.aid.pix,
                                   &aid[SWICC_FS_ADF_AID_RID_LEN],
                                   pix_len) == 0U)
                        {
                            return va_select_file(fs, tree, file_root);
                        }
                    }
                    else
                    {
                        ret = SWICC_RET_ERROR;
                    }
                }
            }
        } while (ret == SWICC_RET_SUCCESS);
    }
    return ret;
}

swicc_ret_et swicc_va_select_file_dfname(swicc_fs_st *const fs,
                                         char const *const df_name,
                                         uint32_t const df_name_len)
{
    return SWICC_RET_UNKNOWN;
}

swicc_ret_et swicc_va_select_file_id(swicc_fs_st *const fs,
                                     swicc_fs_id_kt const fid)
{
    swicc_disk_tree_st *tree;
    swicc_fs_file_st file;
    swicc_ret_et ret = swicc_disk_lutid_lookup(&fs->disk, &tree, fid, &file);
    if (ret == SWICC_RET_SUCCESS)
    {
        return va_select_file(fs, tree, file);
    }
    return ret;
}

swicc_ret_et swicc_va_select_file_sid(swicc_fs_st *const fs,
                                      swicc_fs_sid_kt const sid)
{
    swicc_fs_file_st file;
    swicc_ret_et const ret =
        swicc_disk_lutsid_lookup(fs->va.cur_tree, sid, &file);
    if (ret == SWICC_RET_SUCCESS)
    {
        return va_select_file(fs, fs->va.cur_tree, file);
    }
    return ret;
}

swicc_ret_et swicc_va_select_file_path(swicc_fs_st *const fs,
                                       swicc_fs_path_st const path)
{
    return SWICC_RET_UNKNOWN;
}

swicc_ret_et swicc_va_select_record_idx(swicc_fs_st *const fs,
                                        swicc_fs_rcrd_idx_kt idx)
{
    if (fs->va.cur_ef.hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_EF_LINEARFIXED ||
        fs->va.cur_ef.hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_EF_CYCLIC)
    {
        uint32_t rcrd_cnt;
        if (swicc_disk_file_rcrd_cnt(fs->va.cur_tree, &fs->va.cur_ef,
                                     &rcrd_cnt) == SWICC_RET_SUCCESS)
        {
            swicc_fs_rcrd_st const rcrd = {.idx = idx};
            fs->va.cur_rcrd = rcrd;
            return SWICC_RET_SUCCESS;
        }
    }
    return SWICC_RET_ERROR;
}

swicc_ret_et swicc_va_select_data_offset(swicc_fs_st *const fs,
                                         uint32_t offset_prel)
{
    return SWICC_RET_UNKNOWN;
}

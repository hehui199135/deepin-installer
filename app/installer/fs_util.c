/**
 * Copyright (c) 2011 ~ 2013 Deepin, Inc.
 *               2011 ~ 2013 Long Wei
 *
 * Author:      Long Wei <yilang2007lw@gmail.com>
 * Maintainer:  Long Wei <yilang2007lw@gamil.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 **/

#include "fs_util.h"
#include <stdio.h>
#include <glib/gstdio.h>
#include <mntent.h>

gchar *
get_matched_string (const gchar *target, const gchar *regex_string) 
{
    gchar *result = NULL;
    GError *error = NULL;

    GRegex *regex;
    GMatchInfo *match_info;
    regex = g_regex_new (regex_string, 0, 0, &error);
    if (error != NULL) {
        g_warning ("get matched string:%s\n", error->message);
        g_error_free (error);
        return NULL;
    }

    g_regex_match (regex, target, 0, &match_info);
    if (g_match_info_matches (match_info)) {
        result = g_match_info_fetch (match_info, 0);

    } else {
        g_warning ("get matched string failed!\n");
    }

    g_match_info_free (match_info);
    g_regex_unref (regex);

    return result;
}

gchar *
get_partition_mount_point (const gchar *path)
{
    gchar *mp = NULL;
    struct mntent *ent;
    FILE *mount_file;

    mount_file = setmntent ("/etc/mtab", "r");
    if (mount_file != NULL) {
        while (NULL != (ent = getmntent (mount_file))) {
            if (g_strcmp0 (path, ent->mnt_fsname) == 0) {
                mp = ent->mnt_dir;
                break;
            }
        }

    } else {
        g_warning ("setmntent\n");
    }

    endmntent (mount_file);

    return mp;
}

gdouble 
_get_partition_free_size (const gchar *cmd, const gchar *free_regex, const gchar *free_num_regex, const gchar *unit_regex, const gchar *unit_num_regex) 
{
    gdouble result = 0;

    gchar *output = NULL;
    gint exit_status;
    gchar *free_blocks_line = NULL;
    gchar *free_blocks_num = NULL;
    gchar *block_size_line = NULL;
    gchar *block_size_num = NULL;
    
    gdouble free_double = 0;
    gdouble size_double = 0;

    g_spawn_command_line_sync (cmd, &output, NULL, &exit_status, NULL);
    if (exit_status == -1) {
        g_warning ("run command %s failed\n", cmd);
    }

    free_blocks_line = get_matched_string (output, free_regex);
    free_blocks_num = get_matched_string (free_blocks_line, free_num_regex);
    g_free (free_blocks_line);
    free_double = g_ascii_strtod (free_blocks_num, NULL);
    g_free (free_blocks_num);

    block_size_line = get_matched_string (output, unit_regex);
    block_size_num = get_matched_string (block_size_line, unit_num_regex);
    g_free (block_size_line);
    size_double = g_ascii_strtod(block_size_num, NULL);
    g_free (block_size_num);

    g_free (output);

    result = (free_double * size_double) / (1024 * 1024);

    return result;
}

gchar*
_get_partition_used_size (const gchar *cmd, const gchar *used_regex, const gchar *used_num_regex)
{
    gchar *result = NULL;
    gchar *output = NULL;
    gint exit_status;

    gchar *space = NULL;
    gchar *space_num = NULL;

    g_spawn_command_line_sync (cmd, &output, NULL, &exit_status, NULL);
    if (exit_status == -1) {
        g_warning ("run command %s failed\n", cmd);
    }

    space = get_matched_string (output, used_regex);
    space_num = get_matched_string (space, used_num_regex);
    result = g_strdup (space_num);

    g_free (space);
    g_free (space_num);
    g_free (output);

    return result;
}

gchar*
get_mounted_partition_used (const gchar *path) 
{
    gchar *result = NULL;

    gchar *output = NULL;
    gint exit_status;

    gchar *cmd = g_strdup_printf ("sh -c \"df -h %s | awk '{ print $3 }'\" ", path);

    g_spawn_command_line_sync (cmd, &output, NULL, &exit_status, NULL);
    if (exit_status == -1 ) {
        g_warning ("run command %s failed\n", cmd);
    }
    g_free (cmd);
    result = g_strdup (output);
    g_free (output);

    return result;
}

gchar* 
get_partition_used (const gchar *path, const gchar *fs)
{
    gchar *used = NULL;

    gchar *cmd = NULL;
    gchar *free_regex = NULL;
    gchar *free_num_regex = NULL;
    gchar *unit_regex = NULL;
    gchar *unit_num_regex = NULL;
    gchar *used_regex = NULL;
    gchar *used_num_regex = NULL;

    if (g_str_has_prefix (fs, "ext")) {
        if (g_find_program_in_path ("dumpe2fs") == NULL) {
            g_warning ("dumpe2fs not installed\n");
            return used;
        }
        
        cmd = g_strdup_printf ("dumpe2fs -h %s", path);
        free_regex = g_strdup ("Free blocks:\\s+\\d+");
        free_num_regex = g_strdup ("\\d+");
        unit_regex = g_strdup ("Block size:\\s+\\d+");
        unit_num_regex = g_strdup("\\d+");

        gdouble free = _get_partition_free_size (cmd, free_regex, free_num_regex, unit_regex, unit_num_regex);
        g_free (cmd);
        g_free (free_regex);
        g_free (free_num_regex);
        g_free (unit_regex);
        g_free (unit_num_regex);

        used = g_strdup_printf ("%g", free);

    } else if (g_strcmp0 ("reiserfs", fs) == 0) {
        if (g_find_program_in_path ("debugreiserfs") == NULL) {
            g_warning ("debugreiserfs not installed\n");
            return used;
        }
        
        cmd = g_strdup_printf ("debugreiserfs %s", path);
        free_regex = g_strdup ("Free blocks.*");
        free_num_regex = g_strdup ("\\d+");
        unit_regex = g_strdup ("Blocksize.*");
        unit_num_regex = g_strdup ("\\d+");

        gdouble free = _get_partition_free_size (cmd, free_regex, free_num_regex, unit_regex, unit_num_regex);
        g_free (cmd);
        g_free (free_regex);
        g_free (free_num_regex);
        g_free (unit_regex);
        g_free (unit_num_regex);

        used = g_strdup_printf ("%g", free);

    } else if (g_strcmp0 ("linux-swap", fs) == 0 ) {
        used = NULL;

    } else if (g_strcmp0 ("xfs", fs) == 0) {
        if (g_find_program_in_path ("xfs_db") == NULL) {
            g_warning ("xfs_db not installed\n");
            return used;
        }
            
        cmd = g_strdup_printf ("xfs_db -c 'sb 0' -c 'print blocksize' -c 'print fdblocks' -r %s", path);
        free_regex = g_strdup ("fdblocks.*");
        free_num_regex = g_strdup ("\\d+");
        unit_regex = g_strdup ("blocksize.*");
        unit_num_regex = g_strdup ("\\d+");

        gdouble free = _get_partition_free_size (cmd, free_regex, free_num_regex, unit_regex, unit_num_regex);
        g_free (cmd);
        g_free (free_regex);
        g_free (free_num_regex);
        g_free (unit_regex);
        g_free (unit_num_regex);

        used = g_strdup_printf ("%g", free);

    } else if (g_strcmp0 ("jfs", fs) == 0) {
        if (g_find_program_in_path ("jfs_debugfs") == NULL) {
            g_warning ("jfs_debugfs not installed\n");
            return used;
        }
            
        cmd = g_strdup_printf ("/bin/sh -c 'echo dm | jfs_debugfs %s'", path);
        free_regex = g_strdup ("dn_nfree.*0[xX][0-9a-fA-F]+");
        free_num_regex = g_strdup ("0[xX][0-9a-fA-F]+");
        unit_regex = g_strdup ("Block Size.*");
        unit_num_regex = g_strdup ("\\d+");

        gdouble free = _get_partition_free_size (cmd, free_regex, free_num_regex, unit_regex, unit_num_regex);
        g_free (cmd);
        g_free (free_regex);
        g_free (free_num_regex);
        g_free (unit_regex);
        g_free (unit_num_regex);

        used = g_strdup_printf ("%g", free);

    } else if ( (g_strcmp0 ("fat16", fs) == 0) || (g_strcmp0 ("fat32", fs) == 0)) {
        if (g_find_program_in_path ("dosfsck") == NULL) {
            g_warning ("dosfsck not installed\n");
            return used;
        }
            
       cmd = g_strdup_printf ("dosfsck -n -v %s", path);
        //attention ,in fact this is used space ,not unused
       free_regex = g_strdup (".*\\d+/\\d+");
       free_num_regex = g_strdup ("\\d+/");
       unit_regex = g_strdup ("\\d+ bytes per cluster");
       unit_num_regex = g_strdup ("\\d+");

       gdouble free = _get_partition_free_size (cmd, free_regex, free_num_regex, unit_regex, unit_num_regex);
       g_free (cmd);
       g_free (free_regex);
       g_free (free_num_regex);
       g_free (unit_regex);
       g_free (unit_num_regex);

       used = g_strdup_printf ("%g", free);

    } else if (g_strcmp0 ("btrfs", fs) == 0 ) {
        if (g_find_program_in_path ("btrfs") == NULL) {
            g_warning ("btrfs not installed\n");
            return used;
        }

        cmd = g_strdup_printf ("btrfs filesystem show %s", path);
        used_regex = g_strdup ("used.*path");
        used_num_regex = g_strdup ("\\d+.*MB");

        used = _get_partition_used_size (cmd, used_regex, used_num_regex);
        g_free (cmd);
        g_free (used_regex);
        g_free (used_num_regex);

    } else if (g_strcmp0 ("ntfs", fs) == 0) {
        if (g_find_program_in_path ("ntfsresize") == NULL) {
            g_warning ("ntfsresize not installed\n");
            return used;
        }

        cmd = g_strdup_printf ("ntfsresize -i -f -P %s", path);
        used_regex = g_strdup ("Space in use.*");
        used_num_regex = g_strdup ("\\d+.*MB");

        used = _get_partition_used_size (cmd, used_regex, used_num_regex);
        g_free (cmd);
        g_free (used_regex);
        g_free (used_num_regex);

    } else {
        used =  g_strdup("80G");
    }

    return used;
}

void 
set_partition_filesystem (const gchar *path, const gchar *fs)
{
    gchar *cmd = NULL;
    GError *error = NULL;

    if (g_strcmp0 (fs, "ext4") == 0) {
        if (g_find_program_in_path ("mkfs.ext4") == NULL) {
            g_warning ("mkfs.ext4 not installed\n");
            return ;
        }
        cmd = g_strdup_printf ("mkfs.ext4 %s", path);

    } else if (g_strcmp0 (fs, "ext3") == 0) {
        if (g_find_program_in_path ("mkfs.ext3") == NULL) {
            g_warning ("mkfs.ext3 not installed\n");
            return ;
        }
        cmd = g_strdup_printf ("mkfs.ext3 %s", path);

    } else if (g_strcmp0 (fs, "ext2") == 0) {
        if (g_find_program_in_path ("mkfs.ext2") == NULL) {
            g_warning ("mkfs.ext2 not installed\n");
            return ;
        }
        cmd = g_strdup_printf ("mkfs.ext2 %s", path);

    } else if (g_strcmp0 (fs, "fat16") == 0) {
        if (g_find_program_in_path ("mkdosfs") == NULL) {
            g_warning ("mkdosfs not installed\n");
            return ;
        }
        cmd = g_strdup_printf ("mkdosfs -F16 %s", path);

    } else if (g_strcmp0 (fs, "fat32") == 0) {
        if (g_find_program_in_path ("mkdosfs") == NULL) {
            g_warning ("mkdosfs not installed\n");
            return ;
        }
        cmd = g_strdup_printf ("mkdosfs -F32 %s", path);

    } else if  (g_strcmp0 (fs, "jfs") == 0) {
        if (g_find_program_in_path ("mkfs.jfs") == NULL) {
            g_warning ("mkfs.jfs not installed\n");
            return ;
        }
        cmd = g_strdup_printf ("mkfs.jfs -q %s", path);

    } else if  (g_strcmp0 (fs, "linux-swap") == 0) {
        if (g_find_program_in_path ("mkswap") == NULL) {
            g_warning ("mkswap not installed\n");
            return ;
        }
        cmd = g_strdup_printf ("mkswap %s", path);

    } else if (g_strcmp0 (fs, "ntfs") == 0) {
        if (g_find_program_in_path ("mkntfs") == NULL) {
            g_warning ("mkntfs not installed\n");
            return ;
        }
        cmd = g_strdup_printf ("mkntfs -Q -v %s", path);
    
    } else if (g_strcmp0 (fs, "reiserfs") == 0) {
        if (g_find_program_in_path ("mkreiserfs") == NULL) {
            g_warning ("mkreiserfs not installed\n");
            return ;
        }
        cmd = g_strdup_printf ("mkreiserfs -f %s", path);

    } else if (g_strcmp0 (fs, "btrfs") == 0) {
        if (g_find_program_in_path ("mkfs.btrfs") == NULL) {
            g_warning ("mkfs.btrfs not installed\n");
            return ;
        }
        cmd = g_strdup_printf ("mkfs.btrfs %s", path);

    } else if (g_strcmp0 (fs, "xfs") == 0 ) {
        if (g_find_program_in_path ("mkfs.xfs") == NULL) {
            g_warning ("mkfs.xfs not installed\n");
            return ;
        }
        cmd = g_strdup_printf ("mkfs.xfs -f %s", path);

    } else {
        g_warning ("file system:%s currently not supported\n", fs);
        return ;
    }

    g_spawn_command_line_async (cmd, &error);
    if (error != NULL) {
        g_warning ("%s\n", error->message);
        g_error_free (error);
    }

    g_free (cmd);
}

gchar *
get_partition_label (const gchar *path, const gchar *fs)
{
    gchar *label = NULL;

    gchar *cmd = NULL;
    GError *error = NULL;

    if (g_strcmp0 (fs, "ext4") == 0) {
        if (g_find_program_in_path ("e2label") == NULL) {
            g_warning ("e2label not installed\n");

        } else {
            cmd = g_strdup_printf ("e2label %s", path);
            g_spawn_command_line_sync (cmd, &label, NULL, NULL, &error);
        }

    } else if (g_strcmp0 (fs, "ext3") == 0) {
        if (g_find_program_in_path ("e2label") == NULL) {
            g_warning ("e2label not installed\n");

        } else {
            cmd = g_strdup_printf ("e2label %s", path);
            g_spawn_command_line_sync (cmd, &label, NULL, NULL, &error);
        }

    } else if (g_strcmp0 (fs, "ext2") == 0) {
        if (g_find_program_in_path ("e2label") == NULL) {
            g_warning ("e2label not installed\n");

        } else {
            cmd = g_strdup_printf ("e2label %s", path);
            g_spawn_command_line_sync (cmd, &label, NULL, NULL, &error);
        }

    } else if (g_strcmp0 (fs, "fat16") == 0) {
        g_printf ("to be implemented\n");

    } else if (g_strcmp0 (fs, "fat32") == 0) {
        g_printf ("to be implemented\n");

    } else if  (g_strcmp0 (fs, "jfs") == 0) {
        if (g_find_program_in_path ("jfs_tune") == NULL) {
            g_warning ("jfs_tune not installed\n");

        } else {
            cmd = g_strdup_printf ("jfs_tune -l %s", path);
            g_spawn_command_line_sync (cmd, &label, NULL, NULL, &error);
        }

    } else if  (g_strcmp0 (fs, "linux-swap") == 0) {
        if (g_find_program_in_path ("swaplabel") == NULL) {
            g_warning ("swaplabel not installed\n");

        } else {
            cmd = g_strdup_printf ("swaplabel %s", path);
            g_spawn_command_line_sync (cmd, &label, NULL, NULL, &error);
        }

    } else if (g_strcmp0 (fs, "ntfs") == 0) {
        if (g_find_program_in_path ("ntfslabel") == NULL) {
            g_warning ("ntfslabel not installed\n");

        } else {
            cmd = g_strdup_printf ("ntfslabel --force %s", path);
            g_spawn_command_line_sync (cmd, &label, NULL, NULL, &error);
        }
    
    } else if (g_strcmp0 (fs, "reiserfs") == 0) {
        if (g_find_program_in_path ("debugreiserfs") == NULL) {
            g_warning ("debugreiserfs not installed\n");

        } else {
            cmd = g_strdup_printf ("debugreiserfs %s", path);
            g_spawn_command_line_sync (cmd, &label, NULL, NULL, &error);
        }

    } else if (g_strcmp0 (fs, "xfs") == 0 ) {
        if (g_find_program_in_path ("xfs_db") == NULL) {
            g_warning ("xfs_db not installed\n");

        } else {
            cmd = g_strdup_printf ("xfs_db -r -c 'label' %s", path);
            g_spawn_command_line_sync (cmd, &label, NULL, NULL, &error);
        }

    } else if (g_strcmp0 (fs, "btrfs") == 0 ) {
        if (g_find_program_in_path ("btrfs") == NULL) {
            g_warning ("btrfs not installed\n");

        } else {
            cmd = g_strdup_printf ("btrfs filesytem show %s", path);
            g_spawn_command_line_sync (cmd, &label, NULL, NULL, &error);
        }

    } else {
        g_warning ("file system:%s currently not supported\n", fs);
    }

    if (error != NULL) {
        g_warning ("%s\n", error->message);
        g_error_free (error);
    }
    g_free (cmd);

    return label;
}

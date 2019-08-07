/*
   Chattr command -- for the Midnight Commander

   Copyright (C) 2019
   Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file chattr.c
 *  \brief Source: chattr command
 */

#include <config.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <e2p/e2p.h>
#include <ext2fs/ext2_fs.h>

#include "lib/global.h"

#include "lib/tty/tty.h"        /* COLS */
#include "lib/tty/color.h"      /* tty_setcolor() */
#include "lib/skin.h"           /* COLOR_NORMAL */
#include "lib/vfs/vfs.h"
#include "lib/widget.h"

#include "midnight.h"           /* current_panel */
#include "panel.h"              /* do_file_mark() */

#include "chattr.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define B_MARKED B_USER
#define B_SETALL (B_USER + 1)
#define B_SETMRK (B_USER + 2)
#define B_CLRMRK (B_USER + 3)

#define BUTTONS  6

#define CHATTRBOXES(x) ((WChattrBoxes *)(x))

/*** file scope type declarations ****************************************************************/

typedef struct WChattrBoxes WChattrBoxes;

struct WChattrBoxes
{
    WGroup base;                /* base class */

    int pos;                    /* The current checkbox selected */
    int top;                    /* The first flag displayed */
};

/*** file scope variables ************************************************************************/

/* see /usr/include/ext2fs/ext2_fs.h
 *
 * EXT2_SECRM_FL            0x00000001 -- Secure deletion
 * EXT2_UNRM_FL             0x00000002 -- Undelete
 * EXT2_COMPR_FL            0x00000004 -- Compress file
 * EXT2_SYNC_FL             0x00000008 -- Synchronous updates
 * EXT2_IMMUTABLE_FL        0x00000010 -- Immutable file
 * EXT2_APPEND_FL           0x00000020 -- writes to file may only append
 * EXT2_NODUMP_FL           0x00000040 -- do not dump file
 * EXT2_NOATIME_FL          0x00000080 -- do not update atime
 * * Reserved for compression usage...
 * EXT2_DIRTY_FL            0x00000100
 * EXT2_COMPRBLK_FL         0x00000200 -- One or more compressed clusters
 * EXT2_NOCOMPR_FL          0x00000400 -- Access raw compressed data
 * * nb: was previously EXT2_ECOMPR_FL
 * EXT4_ENCRYPT_FL          0x00000800 -- encrypted inode
 * * End compression flags --- maybe not all used
 * EXT2_BTREE_FL            0x00001000 -- btree format dir
 * EXT2_INDEX_FL            0x00001000 -- hash-indexed directory
 * EXT2_IMAGIC_FL           0x00002000
 * EXT3_JOURNAL_DATA_FL     0x00004000 -- file data should be journaled
 * EXT2_NOTAIL_FL           0x00008000 -- file tail should not be merged
 * EXT2_DIRSYNC_FL          0x00010000 -- Synchronous directory modifications
 * EXT2_TOPDIR_FL           0x00020000 -- Top of directory hierarchies
 * EXT4_HUGE_FILE_FL        0x00040000 -- Set to each huge file
 * EXT4_EXTENTS_FL          0x00080000 -- Inode uses extents
 * EXT4_VERITY_FL           0x00100000 -- Verity protected inode
 * EXT4_EA_INODE_FL         0x00200000 -- Inode used for large EA
 * EXT4_EOFBLOCKS_FL        0x00400000 was here, unused
 * FS_NOCOW_FL              0x00800000 -- Do not cow file
 * EXT4_SNAPFILE_FL         0x01000000 -- Inode is a snapshot
 *                          0x02000000 -- unused yet
 * EXT4_SNAPFILE_DELETED_FL 0x04000000 -- Snapshot is being deleted
 * EXT4_SNAPFILE_SHRUNK_FL  0x08000000 -- Snapshot shrink has completed
 * EXT4_INLINE_DATA_FL      0x10000000 -- Inode has inline data
 * EXT4_PROJINHERIT_FL      0x20000000 -- Create with parents projid
 * EXT4_CASEFOLD_FL         0x40000000 -- Casefolded file
 *                          0x80000000 -- unused yet
 */

static struct
{
    unsigned long flags;
    char attr;
    const char *text;
    gboolean selected;
    gboolean state;             /* state of checkboxes */
} check_attr[] =
{
    /* *INDENT-OFF* */
    { EXT2_SECRM_FL,        's', N_("Secure deletion"),               FALSE, FALSE },
    { EXT2_UNRM_FL,         'u', N_("Undelete"),                      FALSE, FALSE },
    { EXT2_SYNC_FL,         'S', N_("Synchronous updates"),           FALSE, FALSE },
    { EXT2_DIRSYNC_FL,      'D', N_("Synchronous directory updates"), FALSE, FALSE },
    { EXT2_IMMUTABLE_FL,    'i', N_("Immutable"),                     FALSE, FALSE },
    { EXT2_APPEND_FL,       'a', N_("Append only"),                   FALSE, FALSE },
    { EXT2_NODUMP_FL,       'd', N_("No dump"),                       FALSE, FALSE },
    { EXT2_NOATIME_FL,      'A', N_("No update atime"),               FALSE, FALSE },
    { EXT2_COMPR_FL,        'c', N_("Compress"),                      FALSE, FALSE },
#ifdef EXT2_COMPRBLK_FL
    /* removed in v1.43-WIP-2015-05-18
       ext2fsprogs 4a05268cf86f7138c78d80a53f7e162f32128a3d 2015-04-12 */
    { EXT2_COMPRBLK_FL,     'B', N_("Compressed clusters"),           FALSE, FALSE },
#endif
#ifdef EXT2_DIRTY_FL
    /* removed in v1.43-WIP-2015-05-18
       ext2fsprogs 4a05268cf86f7138c78d80a53f7e162f32128a3d 2015-04-12 */
    { EXT2_DIRTY_FL,        'Z', N_("Compressed dirty file"),         FALSE, FALSE },
#endif
#ifdef EXT2_NOCOMPR_FL
    /* removed in v1.43-WIP-2015-05-18
       ext2fsprogs 4a05268cf86f7138c78d80a53f7e162f32128a3d 2015-04-12 */
    { EXT2_NOCOMPR_FL,      'X', N_("Compression raw access"),        FALSE, FALSE },
#endif
#ifdef EXT4_ENCRYPT_FL
    { EXT4_ENCRYPT_FL,      'E', N_("Encrypted inode"),               FALSE, FALSE },
#endif
    { EXT3_JOURNAL_DATA_FL, 'j', N_("Journaled data"),                FALSE, FALSE },
    { EXT2_INDEX_FL,        'I', N_("Indexed directory"),             FALSE, FALSE },
    { EXT2_NOTAIL_FL,       't', N_("No tail merging"),               FALSE, FALSE },
    { EXT2_TOPDIR_FL,       'T', N_("Top of directory hierarchies"),  FALSE, FALSE },
    { EXT4_EXTENTS_FL,      'e', N_("Inode uses extents"),            FALSE, FALSE },
#ifdef EXT4_HUGE_FILE_FL
    /* removed in v1.43.9
       ext2fsprogs 4825daeb0228e556444d199274b08c499ac3706c 2018-02-06 */
    { EXT4_HUGE_FILE_FL,    'h', N_("Huge_file"),                     FALSE, FALSE },
#endif
    { FS_NOCOW_FL,          'C', N_("No COW"),                        FALSE, FALSE },
#ifdef EXT4_CASEFOLD_FL
    /* added in v1.45.0
       ext2fsprogs 1378bb6515e98a27f0f5c220381d49d20544204e 2018-12-01 */
    { EXT4_CASEFOLD_FL,     'F', N_("Casefolded file"),               FALSE, FALSE },
#endif
#ifdef EXT4_INLINE_DATA_FL
    { EXT4_INLINE_DATA_FL,  'N', N_("Inode has inline data"),         FALSE, FALSE },
#endif
#ifdef EXT4_PROJINHERIT_FL
    /* added in v1.43-WIP-2016-05-12
       ext2fsprogs e1cec4464bdaf93ea609de43c5cdeb6a1f553483 2016-03-07
                   97d7e2fdb2ebec70c3124c1a6370d28ec02efad0 2016-05-09 */
    { EXT4_PROJINHERIT_FL,  'P', N_("Project hierarchy")              FALSE, FALSE },
#endif
#ifdef EXT4_VERITY_FL
    /* added in v1.44.4
       ext2fsprogs faae7aa00df0abe7c6151fc4947aa6501b981ee1 2018-08-14
       v1.44.5
       ext2fsprogs 7e5a95e3d59719361661086ec7188ca6e674f139 2018-08-21 */
    { EXT4_VERITY_FL,       'V', N_("Verity protected inode"),        FALSE, FALSE }
#endif
    /* *INDENT-ON* */
};

/* number of attributes */
static const size_t ATTR_NUM = G_N_ELEMENTS (check_attr);

static char attr_str[32 + 1];   /* 32 bits in attributes (unsigned long) */

/* modifable attribute nubers */
static int check_attr_num = 0;  /* 0..31 */
static int check_attr_mod[32];

/* maximum width of attribute text */
static int check_attr_width = 0;

static struct
{
    int ret_cmd;
    button_flags_t flags;
    int width;
    const char *text;
    Widget *button;
} chattr_but[BUTTONS] =
{
    /* *INDENT-OFF* */
    /* 0 */ { B_SETALL, NORMAL_BUTTON, 0, N_("Set &all"),      NULL },
    /* 1 */ { B_MARKED, NORMAL_BUTTON, 0, N_("&Marked all"),   NULL },
    /* 2 */ { B_SETMRK, NORMAL_BUTTON, 0, N_("S&et marked"),   NULL },
    /* 3 */ { B_CLRMRK, NORMAL_BUTTON, 0, N_("C&lear marked"), NULL },
    /* 4 */ { B_ENTER, DEFPUSH_BUTTON, 0, N_("&Set"),          NULL },
    /* 5 */ { B_CANCEL, NORMAL_BUTTON, 0, N_("&Cancel"),       NULL }
    /* *INDENT-ON* */
};

static gboolean flags_changed;
static int current_file;
static gboolean ignore_all;

static unsigned long and_mask, or_mask, flags;

static WLabel *file_attr;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
chattr_fill_str (unsigned long attr)
{
    size_t i;

    for (i = 0; i < ATTR_NUM; i++)
        attr_str[i] = (attr & check_attr[i].flags) != 0 ? check_attr[i].attr : '-';

    attr_str[ATTR_NUM] = '\0';
}

/* --------------------------------------------------------------------------------------------- */

static void
chattr_draw_select (const Widget * w, gboolean selected)
{
    widget_move (w, 0, -1);
    tty_print_char (selected ? '*' : ' ');
    widget_move (w, 0, 1);
}

/* --------------------------------------------------------------------------------------------- */

static void
chattr_toggle_select (const WChattrBoxes * cb, int Id)
{
    Widget *w;

    check_attr[Id].selected = !check_attr[Id].selected;

    /* find checkbox */
    w = WIDGET (g_list_nth_data (GROUP (cb)->widgets, Id - cb->top));

    tty_setcolor (COLOR_NORMAL);
    chattr_draw_select (w, check_attr[Id].selected);
}

/* --------------------------------------------------------------------------------------------- */

static void
chattrboxes_draw (WChattrBoxes * cb)
{
    Widget *w = WIDGET (cb);
    int i;
    GList *l;
    const int *colors;

    colors = widget_get_colors (w);
    tty_setcolor (colors[DLG_COLOR_NORMAL]);
    tty_fill_region (w->y, w->x - 1, w->lines, w->cols + 1, ' ');

    /* redraw checkboxes */
    group_default_callback (w, NULL, MSG_DRAW, 0, NULL);

    /* mark selected checkboxes */
    tty_setcolor (colors[DLG_COLOR_NORMAL]);
    for (i = 0, l = GROUP (cb)->widgets; l != NULL; i++, l = g_list_next (l))
        chattr_draw_select (WIDGET (l->data), check_attr[cb->top + i].selected);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chattrboxes_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WChattrBoxes *cb = CHATTRBOXES (w);
    WGroup *g = GROUP (w);

    switch (msg)
    {
    case MSG_DRAW:
        chattrboxes_draw (cb);
        return MSG_HANDLED;

    case MSG_NOTIFY:
        {
            /* handle checkboxes */
            int i;

            i = g_list_index (g->widgets, sender);
            if (i >= 0)
            {
                int m;

                i += cb->top;
                m = check_attr_mod[i];
                flags ^= check_attr[m].flags;
                chattr_fill_str (flags);
                label_set_textv (file_attr, "%s: %s", (char *) (DIALOG (w->owner)->data), attr_str);
                chattr_toggle_select (cb, i);
                flags_changed = TRUE;
                return MSG_HANDLED;
            }
        }

        return MSG_NOT_HANDLED;

    case MSG_KEY:
        {
            GList *l;

            switch (parm)
            {
                /* TODO: up, down, pgup, pgdown, home, end */
            case KEY_UP:
            case KEY_LEFT:
                /* TODO */
                l = g_list_previous (g->current);

                if (l != NULL)
                {
                    widget_select (WIDGET (l->data));
                    return MSG_HANDLED;
                }
                return MSG_NOT_HANDLED;

            case KEY_DOWN:
            case KEY_RIGHT:
                /* TODO */
                l = g_list_next (g->current);

                if (l != NULL)
                {
                    widget_select (WIDGET (l->data));
                    return MSG_HANDLED;
                }
                return MSG_NOT_HANDLED;

            case KEY_PPAGE:
            case KEY_NPAGE:
                /* TODO */
                return MSG_NOT_HANDLED;

            case KEY_HOME:
                /* TODO */
                l = g_list_first (g->widgets);
                widget_select (WIDGET (l->data));
                return MSG_HANDLED;

            case KEY_END:
                /* TODO */
                l = g_list_last (g->widgets);
                widget_select (WIDGET (l->data));
                return MSG_HANDLED;

            case 'T':
            case 't':
            case KEY_IC:
                chattr_toggle_select (cb, cb->pos);
                if (parm == KEY_IC)
                {
                    /* TODO: down, or scroll down, or go out */
                    group_select_next_widget (g);
                }
                return MSG_HANDLED;

            default:
                return MSG_NOT_HANDLED;
            }
        }

#if 0
    case MSG_CURSOR:
        /* TODO */
        return MSG_NOT_HANDLED;

    case MSG_FOCUS:
        /* TODO */
        {
            Widget *c;

            c = WIDGET (g_list_nth_data (g->widgets, cb->pos - cb->top));
            widget_select (c);

            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;
#endif

    default:
        return group_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
chattrboxes_mouse_callback (Widget * w, mouse_msg_t msg, mouse_event_t * event)
{
    WChattrBoxes *cb = CHATTRBOXES (w);
}

/* --------------------------------------------------------------------------------------------- */

WChattrBoxes *
chattrboxes_new (int y, int x, int height, int width, unsigned long attr)
{
    WChattrBoxes *cb;
    Widget *w;

    if (height <= 0)
        height = 1;

    cb = g_new0 (WChattrBoxes, 1);
    w = WIDGET (cb);
    group_init (GROUP (cb), y, x, height, width, chattrboxes_callback, chattrboxes_mouse_callback);
    w->options |= WOP_SELECTABLE;
    w->keymap = NULL;           /* FIXME */

    cb->pos = 0;
    cb->top = 0;

    return cb;
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
chattr_is_modifiable (size_t i)
{
    return ((check_attr[i].flags & EXT2_FL_USER_MODIFIABLE) != 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
chattr_init (void)
{
    static gboolean i18n = FALSE;
    size_t i;
    int width;

    for (i = 0; i < ATTR_NUM; i++)
        check_attr[i].selected = FALSE;

    if (i18n)
        return;

    i18n = TRUE;

    for (i = 0; i < ATTR_NUM; i++)
        if (chattr_is_modifiable (i))
        {
#ifdef ENABLE_NLS
            check_attr[i].text = _(check_attr[i].text);
#endif

            check_attr_mod[check_attr_num++] = i;

            width = str_term_width1 (check_attr[i].text);
            check_attr_width = MAX (check_attr_width, width);
        }

    check_attr_width += 1 + 3 + 1;      /* mark, [x] and space */

    for (i = 0; i < BUTTONS; i++)
    {
#ifdef ENABLE_NLS
        chattr_but[i].text = _(chattr_but[i].text);
#endif

        chattr_but[i].width = str_term_width1 (chattr_but[i].text) + 3; /* [], spaces and w/o & */
        if (chattr_but[i].flags == DEFPUSH_BUTTON)
            chattr_but[i].width += 2;   /* <> */
    }
}

/* --------------------------------------------------------------------------------------------- */

static WDialog *
chattr_dlg_create (const char *fname, unsigned long attr)
{
    gboolean single_set;
    WDialog *ch_dlg;
    int lines, cols;
    int checkboxes_lines;
    size_t i;
    int y;
    Widget *dw;
    WGroup *dg, *cbg;
    WChattrBoxes *cb;

    /* prepate to set up checkboxe states */
    for (i = 0; i < ATTR_NUM; i++)
        check_attr[i].state = chattr_is_modifiable (i) && (attr & check_attr[i].flags) != 0;

    flags_changed = FALSE;

    single_set = (current_panel->marked < 2);

    cols = check_attr_width;

    checkboxes_lines = check_attr_num;
    lines = 4 + checkboxes_lines + 4;
    if (!single_set)
        lines += 3;

    if (lines >= LINES - 2)
    {
        int dl;

        dl = lines - (LINES - 2);
        lines -= dl;
        checkboxes_lines -= dl;
    }

    ch_dlg =
        dlg_create (TRUE, 0, 0, lines, cols + 6, WPOS_CENTER, FALSE, dialog_colors,
                    dlg_default_callback, NULL, "[Chattr]", _("Chattr command"));
    dg = GROUP (ch_dlg);
    dw = WIDGET (ch_dlg);

    y = 2;
    file_attr = label_new (y++, 3, NULL);
    group_add_widget (dg, file_attr);
    group_add_widget (dg, hline_new (y++, -1, -1));

    cb = chattrboxes_new (y++, 3, checkboxes_lines, check_attr_width, attr);
    cbg = GROUP (cb);
    group_add_widget (dg, cb);

    /* create checkboxes */
    for (i = 0; i < check_attr_num && i < checkboxes_lines; i++)
    {
        int m;
        WCheck *check;

        m = check_attr_mod[i];

        check = check_new (i, 0, check_attr[m].state, check_attr[m].text);
        group_add_widget (cbg, check);
    }

    y += i - 1;

    /* show attributes that are set up */
    chattr_fill_str (attr);

    for (i = single_set ? (BUTTONS - 2) : 0; i < BUTTONS; i++)
    {
        if (i == 0 || i == BUTTONS - 2)
            group_add_widget (dg, hline_new (y++, -1, -1));

        chattr_but[i].button = WIDGET (button_new (y, dw->cols / 2 + 1 - chattr_but[i].width,
                                                   chattr_but[i].ret_cmd, chattr_but[i].flags,
                                                   chattr_but[i].text, NULL));
        group_add_widget (dg, chattr_but[i].button);

        i++;
        chattr_but[i].button = WIDGET (button_new (y++, dw->cols / 2 + 2, chattr_but[i].ret_cmd,
                                                   chattr_but[i].flags, chattr_but[i].text, NULL));
        group_add_widget (dg, chattr_but[i].button);

        /* two buttons in a row */
        cols = MAX (cols, chattr_but[i - 1].button->cols + 1 + chattr_but[i].button->cols);
    }

    label_set_textv (file_attr, "%s: %s", fname, attr_str);
    cols = MAX (cols, WIDGET (file_attr)->cols);

    /* adjust dialog size and button positions */
    if (cols > check_attr_width)
    {
        widget_set_size (dw, dw->y, dw->x, lines, cols + 6);

        /* dialog center */
        cols = dw->x + dw->cols / 2 + 1;

        for (i = single_set ? (BUTTONS - 2) : 0; i < BUTTONS; i++)
        {
            Widget *b;

            b = chattr_but[i++].button;
            widget_set_size (b, b->y, cols - b->cols, b->lines, b->cols);

            b = chattr_but[i].button;
            widget_set_size (b, b->y, cols + 1, b->lines, b->cols);
        }
    }

    /* select first checkbox */
    cbg->current = cbg->widgets;
    widget_select (WIDGET (cb));

    /* file name is used in MSG_NOTIFY handling */
    ch_dlg->data = (void *) fname;

    return ch_dlg;
}

/* --------------------------------------------------------------------------------------------- */

static void
chattr_done (gboolean need_update)
{
    if (need_update)
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

static const char *
next_file (void)
{
    while (!current_panel->dir.list[current_file].f.marked)
        current_file++;

    return current_panel->dir.list[current_file].fname;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
try_chattr (const char *p, unsigned long m)
{
    while (fsetflags (p, m) == -1 && !ignore_all)
    {
        int my_errno = errno;
        int result;
        char *msg;

        msg =
            g_strdup_printf (_("Cannot chattr \"%s\"\n%s"), x_basename (p),
                             unix_error_string (my_errno));
        result =
            query_dialog (MSG_ERROR, msg, D_ERROR, 4, _("&Ignore"), _("Ignore &all"), _("&Retry"),
                          _("&Cancel"));
        g_free (msg);

        switch (result)
        {
        case 0:
            /* try next file */
            return TRUE;

        case 1:
            ignore_all = TRUE;
            /* try next file */
            return TRUE;

        case 2:
            /* retry this file */
            break;

        case 3:
        default:
            /* stop remain files processing */
            return FALSE;
        }
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
do_chattr (const vfs_path_t * p, unsigned long m)
{
    gboolean ret;

    m &= and_mask;
    m |= or_mask;

    ret = try_chattr (vfs_path_as_str (p), m);

    do_file_mark (current_panel, current_file, 0);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static void
chattr_apply_mask (vfs_path_t * vpath, unsigned long m)
{
    gboolean ok;

    if (!do_chattr (vpath, m))
        return;

    do
    {
        const char *fname;

        fname = next_file ();
        vpath = vfs_path_from_str (fname);
        ok = (fgetflags (fname, &m) == 0);

        if (!ok)
        {
            /* if current file was deleted outside mc -- try next file */
            /* decrease current_panel->marked */
            do_file_mark (current_panel, current_file, 0);

            /* try next file */
            ok = TRUE;
        }
        else
        {
            flags = m;
            ok = do_chattr (vpath, m);
        }

        vfs_path_free (vpath);
    }
    while (ok && current_panel->marked != 0);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
chattr_cmd (void)
{
    gboolean need_update = FALSE;
    gboolean end_chattr = FALSE;

    chattr_init ();

    current_file = 0;
    ignore_all = FALSE;

    do
    {                           /* do while any files remaining */
        vfs_path_t *vpath;
        WDialog *ch_dlg;
        const char *fname, *fname2;
        size_t i;
        int result;

        if (!vfs_current_is_local ())
        {
            message (D_ERROR, MSG_ERROR, "%s",
                     _("Cannot change attributes on non-local filesystems"));
            break;
        }

        do_refresh ();

        need_update = FALSE;
        end_chattr = FALSE;

        if (current_panel->marked != 0)
            fname = next_file ();       /* next marked file */
        else
            fname = selection (current_panel)->fname;   /* single file */

        vpath = vfs_path_from_str (fname);
        fname2 = vfs_path_as_str (vpath);

        if (fgetflags (fname2, &flags) != 0)
        {
            message (D_ERROR, MSG_ERROR, _("Cannot get flags of \"%s\"\n%s"), fname,
                     unix_error_string (errno));
            vfs_path_free (vpath);
            break;
        }

        ch_dlg = chattr_dlg_create (fname, flags);

        result = dlg_run (ch_dlg);

        switch (result)
        {
        case B_CANCEL:
            end_chattr = TRUE;
            break;

        case B_ENTER:
            if (flags_changed)
            {
                if (current_panel->marked <= 1)
                {
                    /* single or last file */
                    if (fsetflags (fname2, flags) == -1 && !ignore_all)
                        message (D_ERROR, MSG_ERROR, _("Cannot chattr \"%s\"\n%s"), fname,
                                 unix_error_string (errno));
                    end_chattr = TRUE;
                }
                else if (!try_chattr (fname2, flags))
                {
                    /* stop multiple files processing */
                    result = B_CANCEL;
                    end_chattr = TRUE;
                }
            }

            need_update = TRUE;
            break;

        case B_SETALL:
        case B_MARKED:
            or_mask = 0;
            and_mask = ~0;

            for (i = 0; i < ATTR_NUM; i++)
                if (chattr_is_modifiable (i) && (check_attr[i].selected || result == B_SETALL))
                {
                    if (check_attr[i].state)
                        or_mask |= check_attr[i].flags;
                    else
                        and_mask &= ~check_attr[i].flags;
                }

            chattr_apply_mask (vpath, flags);
            need_update = TRUE;
            end_chattr = TRUE;
            break;

        case B_SETMRK:
            or_mask = 0;
            and_mask = ~0;

            for (i = 0; i < ATTR_NUM; i++)
                if (chattr_is_modifiable (i) && check_attr[i].selected)
                    or_mask |= check_attr[i].flags;

            chattr_apply_mask (vpath, flags);
            need_update = TRUE;
            end_chattr = TRUE;
            break;

        case B_CLRMRK:
            or_mask = 0;
            and_mask = ~0;

            for (i = 0; i < ATTR_NUM; i++)
                if (chattr_is_modifiable (i) && check_attr[i].selected)
                    and_mask &= ~check_attr[i].flags;

            chattr_apply_mask (vpath, flags);
            need_update = TRUE;
            end_chattr = TRUE;
            break;

        default:
            break;
        }

        if (current_panel->marked != 0 && result != B_CANCEL)
        {
            do_file_mark (current_panel, current_file, 0);
            need_update = TRUE;
        }

        vfs_path_free (vpath);

        dlg_destroy (ch_dlg);
    }
    while (current_panel->marked != 0 && !end_chattr);

    chattr_done (need_update);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Copyright (c) 2005 PCMan <pcman.tw@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef __GNUG__
#pragma implementation "telnetview.h"
#endif

#include <glib/gi18n.h>

#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "telnetview.h"
#include "telnetcon.h"

#if defined(USE_IPLOOKUP)
#include <algorithm>
#endif

#ifdef USE_IMAGEVIEW
#include <curl/curl.h>
#include <magic.h>
#endif

#if !defined(MOZ_PLUGIN)
#include "mainframe.h"
#include "stringutil.h"
#include "appconfig.h"

#ifdef USE_NOTIFIER
#ifdef USE_LIBNOTIFY
#include <libnotify/notify.h>
#else
#include "notifier/api.h"
#endif
#endif

CMainFrame* CTelnetView::m_pParentFrame = NULL;
#endif /* !defined(MOZ_PLUGIN) */

    CTelnetView::CTelnetView()
: CTermView()
{
	m_LastPreeditLen = 0;
#if defined(USE_IPLOOKUP) && !defined(MOZ_PLUGIN)
    m_pIpSeeker = seeker_new(AppConfig.GetConfigDirPath().append("/qqwry.dat").c_str());
#endif
}

CTelnetView::~CTelnetView()
{
#if defined(USE_IPLOOKUP) && !defined(MOZ_PLUGIN)
    if (m_pIpSeeker) seeker_delete(m_pIpSeeker);
#endif
}

string CTelnetView::m_WebBrowser;
string CTelnetView::m_MailClient;

#ifdef USE_IMAGEVIEW
string CTelnetView::m_ImageViewer;
#endif

#ifdef USE_WGET
bool CTelnetView::m_bWgetFiles = false;
#endif

static GtkWidget* input_menu_item = NULL;

void CTelnetView::OnTextInput(const gchar* text)
{
    gsize l;
    gchar* _text = my_convert(text, strlen(text), GetCon()->m_Site.m_Encoding.c_str(), "UTF-8", &l);
    if( _text )
    {
        ((CTelnetCon*)m_pTermData)->Send(_text, l);
        g_free(_text);
    }
    // clear the old selection
    // Workaround FIXME please
    if (!m_pTermData->m_Sel->Empty())
    {
        GdkEventButton t_PseudoEvent;
        t_PseudoEvent.x = 0;
        t_PseudoEvent.y = 0;
        t_PseudoEvent.type = GDK_BUTTON_PRESS;
        CTermView::OnLButtonDown(&t_PseudoEvent);
        CTermView::OnLButtonUp(&t_PseudoEvent);
    }
}

void CTelnetView::MoveNext(int &row, int &col)
{
	col++;
	if (col == m_pTermData->m_ColsPerPage)
	{
		col = 0;
		row++;
		if (row == m_pTermData->m_RowsPerPage)
		{
			// out of window!
			row--;
		}
	}
}

void CTelnetView::ClearLastPreedit()
{
	int row = m_LastPreeditRow;
	int col = m_LastPreeditCol;

	for (int i=0; i<m_LastPreeditLen; i++)
	{
		DrawChar(row, col);
		MoveNext(row, col);
	}
}

static int get_char_width(gunichar ch)
{
	// is this OK?? FIXME!
	if (ch < 128)
		return 1;
	else
		return 2;
}

void CTelnetView::OnPreedit()
{
	if (!m_pTermData) return;
	UpdateCaretPos();
	gchar *str;
	PangoAttrList *attrs;
	gint pos;
	gtk_im_context_get_preedit_string(m_IMContext, &str, &attrs, &pos);

	if (m_LastPreeditLen > 0)
	{
		ClearLastPreedit();
		m_LastPreeditLen = 0;
	}

	if (str[0])
		if (g_utf8_validate(str, -1, NULL))
		{
			int cp_x = m_pTermData->m_CaretPos.x;
			int cp_y = m_pTermData->m_CaretPos.y;

			m_LastPreeditRow = cp_y;
			m_LastPreeditCol = cp_x;

			gchar* curchar = str;

			int len = 0;
			while (*curchar)
			{
				gunichar curchar_u = g_utf8_get_char(curchar);
				curchar = g_utf8_next_char(curchar);
				int cwid = get_char_width(curchar_u);
				// how about cwid > 2?
				if ((cwid == 2) && (cp_x == m_pTermData->m_ColsPerPage - 1))
				{
					len++;
					MoveNext(cp_y, cp_x);
				}

				DrawCharAt(curchar_u, cp_y, cp_x, cwid);
				for (int i=0; i<cwid; i++)
					MoveNext(cp_y, cp_x);
				len += cwid;
			}

			m_LastPreeditLen = len;

		} else {
			printf("non-UTF8 preedit string detected! Please contact authors!\n");
		}
	//	printf("preedit: %s, %d\n", str, pos);

	g_free(str);
	pango_attr_list_unref(attrs);

}

#define	GDK_MODIFIER_DOWN(key, mod)	(key & (mod|(~GDK_SHIFT_MASK&~GDK_CONTROL_MASK&~GDK_MOD1_MASK)))

static int DrawCharWrapper( int row, int col, void *data )
{
    CTermView *tv = (CTermView *) data;

    return tv->DrawChar( row, col );
}

bool CTelnetView::OnKeyDown(GdkEventKey* evt)
{
    INFO("CTelnetView::OnKeyDown (keyval=0x%x, state=0x%x)", evt->keyval, evt->state);
    CTermCharAttr* pAttr = m_pTermData->GetLineAttr(
            m_pTermData->m_Screen[m_pTermData->m_CaretPos.y] );
    int x = m_pTermData->m_CaretPos.x;
    bool clear = true;

    if( evt->keyval < 127 && GDK_MODIFIER_DOWN(evt->state, GDK_CONTROL_MASK))// Ctrl down
    {
        char ch = toupper(char(evt->keyval));
        if( ch >= '@' && ch <= '_'	&& !isdigit(ch) )
        {
            // clear the old selection
            if (!m_pTermData->m_Sel->Empty())
                ClearSelection();

            ch -= '@';
            GetCon()->SendRawString(&ch,1);
            return true;
        }
    }
    const char* pline;
    switch(evt->keyval)
    {
        case GDK_Left:
        case GDK_KP_Left:
            GetCon()->SendRawString("\x1bOD\x1bOD",( GetCon()->DetectDBChar() && x > 0 && pAttr[x-1].GetCharSet() == CTermCharAttr::CS_MBCS2 ) ? 6 : 3);
            break;
        case GDK_Right:
        case GDK_KP_Right:
            GetCon()->SendRawString("\x1bOC\x1bOC",( GetCon()->DetectDBChar() && pAttr[x].GetCharSet() == CTermCharAttr::CS_MBCS1 ) ? 6 : 3);
            break;
        case GDK_Up:
        case GDK_KP_Up:
            GetCon()->SendRawString("\x1bOA",3);
            break;
        case GDK_Down:
        case GDK_KP_Down:
            GetCon()->SendRawString("\x1bOB",3);
            pline = m_pTermData->m_Screen[23];
            break;
        case GDK_BackSpace:
            GetCon()->SendRawString("\x7f\x7f", ( GetCon()->DetectDBChar() && x > 0 && pAttr[x-1].GetCharSet() == CTermCharAttr::CS_MBCS2 ) ? 2 : 1);
            break;
        case GDK_Return:
        case GDK_KP_Enter:
            GetCon()->SendRawString("\r",1);
            break;
        case GDK_Delete:
        case GDK_KP_Delete:
            GetCon()->SendRawString("\x1b[3~\x1b[3~",( GetCon()->DetectDBChar() && pAttr[x].GetCharSet() == CTermCharAttr::CS_MBCS1 ) ? 8 : 4);
            break;
        case GDK_Insert:
        case GDK_KP_Insert:
            GetCon()->SendRawString("\x1b[2~",4);
            break;
        case GDK_Home:
        case GDK_KP_Home:
            GetCon()->SendRawString("\x1b[1~",4);
            break;
        case GDK_End:
        case GDK_KP_End:
            GetCon()->SendRawString("\x1b[4~",4);
            break;
            //	case GDK_Prior:
        case GDK_Page_Up:
        case GDK_KP_Page_Up:
            GetCon()->SendRawString("\x1b[5~",4);
            break;
            //	case GDK_Next:
        case GDK_Page_Down:
        case GDK_KP_Page_Down:
            GetCon()->SendRawString("\x1b[6~",4);
            break;
        case GDK_Tab:
            GetCon()->SendRawString("\t", 1);
            break;
        case GDK_Escape:
            GetCon()->SendRawString("\x1b", 1);
            break;
            // F1-F12 keys
        case GDK_F1:
        case GDK_KP_F1:
            GetCon()->SendRawString("\x1bOP", 3);
            break;
        case GDK_F2:
        case GDK_KP_F2:
            GetCon()->SendRawString("\x1bOQ", 3);
            break;
        case GDK_F3:
        case GDK_KP_F3:
            GetCon()->SendRawString("\x1bOR", 3);
            break;
        case GDK_F4:
        case GDK_KP_F4:
            GetCon()->SendRawString("\x1bOS", 3);
            break;
        case GDK_F5:
            GetCon()->SendRawString("\x1b[15~", 5);
            break;
        case GDK_F6:
            GetCon()->SendRawString("\x1b[17~", 5);
            break;
        case GDK_F7:
            GetCon()->SendRawString("\x1b[18~", 5);
            break;
        case GDK_F8:
            GetCon()->SendRawString("\x1b[19~", 5);
            break;
        case GDK_F9:
            GetCon()->SendRawString("\x1b[20~", 5);
            break;
        case GDK_F10:
            GetCon()->SendRawString("\x1b[21~", 5);
            break;
        case GDK_F11:
            GetCon()->SendRawString("\x1b[23~", 5);
            break;
        case GDK_F12:
            GetCon()->SendRawString("\x1b[24~", 5);
            break;
        default:
            clear = false;
    }

    // Only clear selection if we handled the key
    if (clear)
        ClearSelection();

    return true;
}

static void on_hyperlink_copy(GtkMenuItem* item UNUSED, bool *do_copy)
{
    *do_copy = true;
}

#if defined(USE_IPLOOKUP) && !defined(MOZ_PLUGIN)
static inline unsigned int ipstr2int(const char *str)
{
    unsigned char ip[4];
    if (sscanf(str, " %hhu . %hhu . %hhu . %hhu"
                , ip + 3, ip + 2, ip + 1, ip) != 4)
        return 0;
    return *((unsigned int*)ip);
}
#endif

void CTelnetView::OnMouseMove(GdkEventMotion* evt)
{
    if( !m_pTermData )
        return;

    int x = (int)evt->x;
    int y = (int)evt->y;
    bool left;
#ifdef USE_NOTIFIER
#ifdef USE_LIBNOTIFY
	static NotifyNotification *ipnotify = NULL;
#endif
#endif

    INFO("x=%d, y=%d, grab=%d", x, y, HasCapture());

    this->PointToLineCol( &x, &y, &left );
    if( HasCapture() )	//	Selecting text.
    {
        if ( m_pTermData->m_Sel->m_End.row != y
                || m_pTermData->m_Sel->m_End.col != x
                || m_pTermData->m_Sel->m_End.left != left )
        {
            // Always remember to hide the caret before drawing.
            m_Caret.Hide();

            m_pTermData->m_Sel->ChangeEnd( y, x, left, DrawCharWrapper, this );

            // Show the caret again but only set its visibility without
            // display it immediatly.
            m_Caret.Show( false );
#ifdef USE_MOUSE
            {gdk_window_set_cursor(m_Widget->window, NULL);m_CursorState=0;}
#endif
        }
    }
#if !defined(MOZ_PLUGIN)
    else
    {
        CTermCharAttr* pattr = m_pTermData->GetLineAttr(m_pTermData->m_Screen[ y ]);

#if defined(USE_IPLOOKUP)
        // Update status bar for ip address lookup.
        m_pParentFrame->PopStatus("show ip");
        if( x > 0 && x < m_pTermData->m_ColsPerPage && pattr[x].IsIpAddr() )
        {
            int ip_beg, ip_end;
            for (ip_beg = x; ip_beg >= 0 && pattr[ip_beg].IsIpAddr(); ip_beg--);
            ip_beg++;
            for (ip_end = x; ip_end < m_pTermData->m_ColsPerPage && pattr[ip_end].IsIpAddr(); ip_end++);
            string ipstr(m_pTermData->m_Screen[y] + ip_beg, ip_end - ip_beg);
            string originalIP = ipstr;
            string::iterator star = find(ipstr.begin(), ipstr.end(), '*');
            while (star != ipstr.end())
            {
                *star = '0';
                star = find(star + 1, ipstr.end(), '*');
            }

            char buf[255];
            if (m_pIpSeeker)
            {
                seeker_lookup(m_pIpSeeker, ipstr2int(ipstr.c_str()), buf, sizeof(buf));
                gchar *location = my_convert(buf, -1, "UTF-8", "GBK", NULL);
                snprintf(buf, sizeof(buf), "%s %s (%s)"
                        , _("Detected IP address:"), originalIP.c_str(), location);
                g_free(location);
            }
            else
                snprintf(buf, sizeof(buf), "%s %s (%s)"
                        , _("Detected IP address:"), ipstr.c_str()
                        , _("Download qqwry.dat to get IP location lookup"));

            m_pParentFrame->PushStatus("show ip", buf);

			SetTooltip(buf);

			if (AppConfig.IPNotifier)
			{
#ifdef USE_NOTIFIER
#ifdef USE_LIBNOTIFY
				if (!ipnotify)
#ifdef NOTIFY_CHECK_VERSION
#if NOTIFY_CHECK_VERSION(0,7,0)
					ipnotify = notify_notification_new(
							buf,
							NULL,
							NULL);
#else
					ipnotify = notify_notification_new(
							buf,
							NULL,
							NULL,
							NULL);
#endif // CHECK_VERSION(0,7,0)
#else
					ipnotify = notify_notification_new(
							buf,
							NULL,
							NULL,
							NULL);
#endif
				else
					notify_notification_update(ipnotify, buf, NULL, NULL);
				notify_notification_set_timeout(ipnotify,
						AppConfig.PopupTimeout * 1000);
				notify_notification_show(ipnotify,
						NULL);
#endif
#endif
			}
        } else {
			SetTooltip(NULL);
			if (AppConfig.IPNotifier)
			{
#ifdef USE_NOTIFIER
#ifdef USE_LIBNOTIFY
				if (ipnotify)
				{
					notify_notification_close(
							ipnotify,
							NULL);
					g_object_unref(G_OBJECT(ipnotify));
					ipnotify = NULL;
				}
#endif
#endif
			}
		}

#endif // defined(USE_IPLOOKUP)

#if defined(USE_MOUSE)
        if ( AppConfig.MouseSupport == true )
        {
            if( x > 0 && x < m_pTermData->m_ColsPerPage && pattr[x].IsHyperLink() )
            {gdk_window_set_cursor(m_Widget->window, m_HandCursor);m_CursorState=-1;}
            else
            {
                switch( ((CTelnetCon*)m_pTermData)->GetPageState() )
                {
                    case -1: //NORMAL
                        gdk_window_set_cursor(m_Widget->window, NULL);
                        m_CursorState=0;
                        break;
                    case 1: //LIST
                        if ( y>2 && y < m_pTermData->m_RowsPerPage-1 )
                        {
                            if ( x <= 6 )
                            {gdk_window_set_cursor(m_Widget->window, m_ExitCursor);m_CursorState=1;}
                            else if ( x >= m_pTermData->m_ColsPerPage-16 )
                            {
                                if ( y > m_pTermData->m_RowsPerPage /2 )
                                {gdk_window_set_cursor(m_Widget->window, m_PageDownCursor);m_CursorState=3;}
                                else
                                {gdk_window_set_cursor(m_Widget->window, m_PageUpCursor);m_CursorState=4;}
                            }
                            else
                            {gdk_window_set_cursor(m_Widget->window, m_BullsEyeCursor);m_CursorState=2;}
                        }
                        else if ( y==1 || y==2 )
                        {gdk_window_set_cursor(m_Widget->window, m_PageUpCursor);m_CursorState=4;}
                        else if ( y==0 )
                        {gdk_window_set_cursor(m_Widget->window, m_HomeCursor);m_CursorState=6;}
                        else //if ( y = m_pTermData->m_RowsPerPage-1)
                        {gdk_window_set_cursor(m_Widget->window, m_EndCursor);m_CursorState=5;}
                        break;
                    case 2: //READING
                        if ( y == m_pTermData->m_RowsPerPage-1)
                        {gdk_window_set_cursor(m_Widget->window, m_EndCursor);m_CursorState=5;}
                        else if ( x<7 )
                        {gdk_window_set_cursor(m_Widget->window, m_ExitCursor);m_CursorState=1;}
                        else if ( y < (m_pTermData->m_RowsPerPage-1)/2 )
                        {gdk_window_set_cursor(m_Widget->window, m_PageUpCursor);m_CursorState=4;}
                        else
                        {gdk_window_set_cursor(m_Widget->window, m_PageDownCursor);m_CursorState=3;}
                        break;
                    case 0: //MENU
                        if ( y>0 && y < m_pTermData->m_RowsPerPage-1 )
                        {
                            if (x>7)
                            {gdk_window_set_cursor(m_Widget->window, m_BullsEyeCursor);m_CursorState=2;}
                            else
                            {gdk_window_set_cursor(m_Widget->window, m_ExitCursor);m_CursorState=1;}
                        }
                        else
                        {gdk_window_set_cursor(m_Widget->window, NULL);m_CursorState=0;}
                        break;
                    default:
                        break;
                }
            }
        }
        else
        {
            CTermCharAttr* pattr = m_pTermData->GetLineAttr(m_pTermData->m_Screen[ y ]);
            if( x > 0 && x < m_pTermData->m_ColsPerPage && pattr[x].IsHyperLink() )
                gdk_window_set_cursor(m_Widget->window, m_HandCursor);
            else
                gdk_window_set_cursor(m_Widget->window, NULL);;
            m_CursorState=0;
        }
#endif // defined(USE_MOUSE)
    }
#endif // !defined(MOZ_PLUGIN)
}

#if defined(USE_MOUSE) && !defined(MOZ_PLUGIN)
void CTelnetView::OnMouseScroll(GdkEventScroll* evt)
{
    if( !m_pTermData )
        return;

    if ( AppConfig.MouseSupport != true )
        return;

    GdkScrollDirection i = evt->direction;;
    if ( i == GDK_SCROLL_UP )
        GetCon()->SendRawString("\x1bOA",3);
    if ( i == GDK_SCROLL_DOWN )
        GetCon()->SendRawString("\x1bOB",3);
}

void CTelnetView::OnLButtonUp(GdkEventButton* evt)
{
    CTermView::OnLButtonUp(evt);

    if( !m_pTermData )
        return;

    if ( AppConfig.MouseSupport != true )
        return;

    int x = (int)evt->x;
    int y = (int)evt->y;
    bool left;
    this->PointToLineCol( &x, &y, &left );

    char buf[4096];
    // Don't send mouse action when the user click on hyperlinks
    if( HyperLinkHitTest( x, y, buf ) )
        return;

    //some text is selected
    if ( m_CancelSel
            || m_pTermData->m_Sel->m_End.row != y
            || m_pTermData->m_Sel->m_End.col != x
            || m_pTermData->m_Sel->m_End.left != left
            || m_pTermData->m_Sel->m_Start.row != y
            || m_pTermData->m_Sel->m_Start.col != x
            || m_pTermData->m_Sel->m_Start.left != left )
        return;

    int cur = m_CursorState;
    int ps = ((CTelnetCon*)m_pTermData)->GetPageState();

    if ( cur == 2 ) // mouse on entering mode
    {
        switch (ps)
        {
            case 1: // list
                {
                    int n = y - m_pTermData->m_CaretPos.y;
                    if ( n>0 )
                        while(n)
                        {
                            GetCon()->SendRawString("\x1bOB",3);
                            n--;
                        }
                    if ( n<0 )
                    {
                        n=-n;
                        while(n)
                        {
                            GetCon()->SendRawString("\x1bOA",3);
                            n--;
                        }
                    }
                    GetCon()->SendRawString("\r",1); //return key
                    break;
                }
            case 0: // menu
                {
                    char cMenu = ((CTelnetCon*)m_pTermData)->GetMenuChar(y);
                    GetCon()->SendRawString( &cMenu, 1 );
                    GetCon()->SendRawString( "\r", 1 );
                    break;
                }
            case -1: // normal
                GetCon()->SendRawString( "\r", 1 );
                break;
            default:
                break;
        }
    }
    else if (cur == 1)
        GetCon()->SendRawString("\x1bOD",3); //exiting mode
    else if (cur == 6)
        GetCon()->SendRawString("\x1b[1~",4); //home
    else if (cur == 5)
        GetCon()->SendRawString("\x1b[4~",4); //end
    else if (cur == 4)
        GetCon()->SendRawString("\x1b[5~",4); //pageup
    else if (cur == 3)
        GetCon()->SendRawString("\x1b[6~",4); //pagedown
    else
        GetCon()->SendRawString( "\r", 1 );
}

void CTelnetView::OnButtonUp(GdkEventButton *evt)
{
    CTermView::OnButtonUp(evt);

    if( !m_pTermData )
        return;

    if ( AppConfig.MouseSupport != true )
        return;

    switch(evt->button)
    {
        case 8: // Navigate backward
            GetCon()->SendRawString("\x1bOD", 3);
            break;
        case 9: // Navigate forward
            GetCon()->SendRawString("\r", 1);
            break;
    }
}
#endif  // defined(USE_MOUSE) && !defined(MOZ_PLUGIN)

void CTelnetView::OnRButtonDown(GdkEventButton* evt)
{
#if !defined(MOZ_PLUGIN)
    if( !m_ContextMenu )
        return;
#endif

    if( m_pTermData )	// Copy URL popup menu.
    {
        int x = (int)evt->x;
        int y = (int)evt->y;
        PointToLineCol( &x, &y );
        char buf[4096];
        if( HyperLinkHitTest( x, y, buf ) )
        {
            char* pline = m_pTermData->m_Screen[y];
            bool do_copy = false;
            // Show the "Copy Hyperlink" menu.
            GtkWidget* popup = gtk_menu_new();
            GtkWidget* item = gtk_image_menu_item_new_with_mnemonic( _("_Copy URL to Clipboard") );
            GtkWidget* icon = gtk_image_new_from_stock ("gtk-copy", GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), icon);
            g_signal_connect( G_OBJECT(item), "activate",
                    G_CALLBACK(on_hyperlink_copy), &do_copy);

            gtk_menu_shell_append  ((GtkMenuShell *)popup, item );
            gtk_widget_show_all(popup);
            g_signal_connect( G_OBJECT(popup), "deactivate",
                    G_CALLBACK(gtk_main_quit), this);
            gtk_menu_popup( (GtkMenu*)popup, NULL, NULL, NULL, NULL, evt->button, evt->time );
            gtk_main();		// Don't return until the menu is closed.

            if( do_copy )
            {
                // Note by Hong Jen Yee (PCMan):
                // Theoratically, there is no non-ASCII characters in standard URL,
                // so we don't need to do UTF-8 conversion at all.
                // However, users are always right.
                string url( buf );
                gsize wl = 0;
                const gchar* purl = my_convert( url.c_str(), url.length(),
                        "UTF-8", m_pTermData->m_Encoding.c_str(), &wl);
                if(purl)
                {
                    m_s_ANSIColorAttr.empty();
                    GtkClipboard* clipboard = gtk_clipboard_get( GDK_NONE );
                    gtk_clipboard_set_text(clipboard, purl, wl );
                    clipboard = gtk_clipboard_get(  GDK_SELECTION_PRIMARY);
                    gtk_clipboard_set_text(clipboard, purl, wl );
                    g_free((void*)purl);
                }
            }
            gtk_widget_destroy(popup);
            return;
        }
    }
#if !defined(MOZ_PLUGIN)
    if( input_menu_item )
        gtk_widget_destroy( input_menu_item );
    input_menu_item = gtk_menu_item_new_with_mnemonic (_("Input _Methods"));
    gtk_widget_show (input_menu_item);
    GtkWidget* submenu = gtk_menu_new ();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (input_menu_item), submenu);

    gtk_menu_shell_append (GTK_MENU_SHELL (m_ContextMenu), input_menu_item);

    gtk_im_multicontext_append_menuitems (GTK_IM_MULTICONTEXT (m_IMContext),
            GTK_MENU_SHELL (submenu));
    gtk_menu_popup( m_ContextMenu, NULL, NULL, NULL, NULL, evt->button, evt->time );
#endif
}

bool CTelnetView::PreKeyDown(GdkEventKey* evt UNUSED)
{
    /*	if( GDK_MODIFIER_DOWN( evt->state, GDK_MOD1_MASK)
        || GDK_MODIFIER_DOWN( evt->state, GDK_CONTROL_MASK)
        && ((evt->keyval > GDK_0 && evt->keyval > GDK_9)
        || (evt->keyval > GDK_KP_0 && evt->keyval > GDK_KP_9) )
        )
        {
        int i = evt->keyval > GDK_KP_0 ? (evt->keyval - GDK_KP_0):(evt->keyval - GDK_0);
        m_pParentFrame->SwitchToTab(i);
        return true;
        }
        */	return false;
}

static string GetChangedAttrStr(CTermCharAttr oldattr, CTermCharAttr newattr, 
        const string & esc)
{
	string text = esc + "[";	// Control sequence introducer.
	bool reset = false;
	 // If we want to cancel bright attribute, we must reset all attributes.
	bool bright_changed = (newattr.IsBright() != oldattr.IsBright());
	if( bright_changed && 1 == oldattr.IsBright() )
		reset = true;
	// Blink attribute changed.
	// We must reset all attributes to remove 'blink' attribute.
	bool blink_changed = (newattr.IsBlink() != oldattr.IsBlink());
	if( blink_changed && 1 == oldattr.IsBlink() )
		reset = true;
	// Underline attribute changed.
	// We must reset all attributes to remove 'underline' attribute.
	bool underline_changed = (newattr.IsUnderLine() != oldattr.IsUnderLine());
	if( underline_changed && 1 == oldattr.IsUnderLine() )
		reset = true;
	// Inverse attribute changed.
	// We must reset all attributes to remove 'inverse' attribute.
	bool inverse_changed = (newattr.IsInverse() != oldattr.IsInverse());
	if( inverse_changed && 1 == oldattr.IsInverse() )
		reset = true;

	if(reset)
		text += "0;";	// remove all attributes
	if( (reset || bright_changed) && newattr.IsBright() )	// Add bright attribute
		text += "1;";
	if( (reset || blink_changed) && newattr.IsBlink() )	// Add blink attribute
		text += "5;";
	if( (reset || underline_changed) && newattr.IsUnderLine() ) // Add underline attributes.
		text += "4;";
	if( (reset || inverse_changed) && newattr.IsInverse() ) // Add inverse attributes.
		text += "7;";
	if( reset || newattr.GetBackground() != oldattr.GetBackground())	// If reset or color changed.
	{
		char color[] = {'4', static_cast<char>('0'+ newattr.GetBackground()), ';', '\0' };
		text += color;
	}
	if( reset || newattr.GetForeground() != oldattr.GetForeground() )
        {
		char color[] = {'3', static_cast<char>('0' + newattr.GetForeground()), ';', '\0' };
		text += color;
	}
	if( ';' == text[ text.length()-1 ] )	// Don't worry about access violation because text.Len() always > 1.
		text = text.substr(0, text.length()-1);
	text += 'm';	// Terminate ANSI escape sequence
	return text;
}

void CTelnetView::DoPasteFromClipboard(string text, bool contain_ansi_color)
{
    if( GetCon() )
    {
        string text2;
        if( contain_ansi_color )
        {
            string esc = GetCon()->m_Site.GetEscapeChar();

            gsize convl;
            gchar* locale_text = my_convert(text.c_str(), 
                    text.length(),GetCon()->m_Site.m_Encoding.c_str(),
                    "UTF-8", &convl);


            INFO("%lx", (unsigned long)locale_text);
            if( !locale_text )
                return;

            text2 += esc + "[m";
            int len = strlen(locale_text);
            if (len > m_s_ANSIColorAttr.size())
                len = m_s_ANSIColorAttr.size();
            int i;
            CTermCharAttr attr;
            attr.SetToDefault();
            for (i = 0; i < len; i++) {
                if (locale_text[i] == '\n') {
                    text2 += esc + "[m\n";
                    attr.SetToDefault();
                } else {
                    if (!attr.IsSameAttr(m_s_ANSIColorAttr[i].AsShort())) {
                        text2 += GetChangedAttrStr(attr, m_s_ANSIColorAttr[i], esc);
                        attr = m_s_ANSIColorAttr[i];
                    }
                    if ( locale_text[i] )
                        text2 += locale_text[i];
                }
            }

            text2 += esc + "[m";
            
            g_free(locale_text);

            GetCon()->SendRawString(text2.c_str(),text2.length());
        }
        else
        {
            // Only when no control character is in this string can
            // autowrap be enabled
            unsigned int len = 0, max_len = GetCon()->m_Site.m_AutoWrapOnPaste;
            gsize convl;
            gchar* locale_text = my_convert(text.c_str(), 
                    text.length(), GetCon()->m_Site.m_Encoding.c_str(), 
                    "UTF-8", &convl);
            if( !locale_text )
                return;
            // FIXME: Convert UTF-8 string to locale string.to prevent invalid UTF-8 string
            // caused by the auto-wrapper.
            // Just a workaround.  This needs to be modified in the future.
            const char* ptext = locale_text;
            const char* crlf = GetCon()->m_Site.GetCRLF();
            if( GetCon()->m_Site.m_AutoWrapOnPaste > 0 )
            {
                string str2;
                const char* pstr = locale_text;
                for( ; *pstr; ++pstr )
                {
                    size_t word_len = 1;
                    const char* pword = pstr;
                    if( ((unsigned char)*pstr) < 128 )		// This is a ASCII character
                    {
                        if( *pstr == '\n' || *pstr == '\r' )
                            len = 0;
                        else
                        {
                            while( *pstr && ((unsigned char)*(pstr+1)) && ((unsigned char)*(pstr+1)) < 128  && !strchr(" \t\n\r", *pstr) )
                                ++pstr;
                            word_len = (pstr - pword) + (*pstr != '\t' ? 1 : 4);	// assume tab width = 4, may be changed in the future
                        }
                    }
                    else
                    {
                        ++pstr;
                        word_len = ( *pstr ? 2 : 1 );
                    }

                    if( (len + word_len) > max_len )
                    {
                        len = 0;
                        str2 += '\n';
                    }
                    len += word_len;
                    while( pword <= pstr )
                    {
                        str2 += *pword;
                        pword ++;
                    }
                    if( *pstr == '\n' || *pstr == '\r' )
                        len = 0;
                }
                text = str2;
                ptext = text.c_str();
            }

            string text2;
            for( const char* pstr = ptext; *pstr; ++pstr )
                if( *pstr == '\n' )
                    text2 += crlf;
                else
                    text2 += *pstr;

            GetCon()->SendRawString(text2.c_str(), text2.length() );

            g_free( locale_text );
        }
    }
}


void CTelnetView::OnDestroy()
{
    if( m_pTermData )
    {
        delete m_pTermData;
        m_pTermData = NULL;
    }
}

#ifdef USE_IMAGEVIEW
#define FILE_HEADER_READ_SIZE 256
struct fileheader
{
    size_t size;
    unsigned char data[FILE_HEADER_READ_SIZE];
};

static size_t fhreader(void *ptr,  size_t size,  size_t nmemb, void *stream)
{
    size_t totsize = size * nmemb;
    struct fileheader * fh = (struct fileheader *)stream;
    size_t i = 0;
    while (fh->size < FILE_HEADER_READ_SIZE && i < totsize) {
        fh->data[fh->size++] = ((unsigned char *)ptr)[i++];
    }
    return i;
}

static bool isImageURL(const char * url)
{
    CURL * curl = curl_easy_init();
    struct fileheader fh;
    bool isImage = false;
    if (curl) {
        CURLcode res = curl_easy_setopt(curl, CURLOPT_URL, url);
        res = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fh);
        res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &fhreader);
        res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
        /*
        res = curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
        res = curl_easy_setopt(curl, CURLOPT_PROXY, "127.0.0.1:7070");
        */

        memset(&fh, 0, sizeof(fh));

        res = curl_easy_perform(curl);

        if (res != CURLE_OK && res != CURLE_WRITE_ERROR)
            return false;

        magic_t mt1 = magic_open(MAGIC_MIME_TYPE);
        magic_t mt2 = magic_open(MAGIC_NONE);
        magic_load(mt1, NULL);
        magic_load(mt2, NULL);

        const char * mimestr = magic_buffer(mt1, fh.data, fh.size);
        const char * typestr = magic_buffer(mt2, fh.data, fh.size);

        fprintf(stderr, "%s %s\n", mimestr, typestr);

        if (strstr(mimestr, "image") == mimestr) {
            isImage = true;
        }

        magic_close(mt1);
        magic_close(mt2);
        curl_easy_cleanup(curl);
    }
    return isImage;
}

static void thr_downimage(string app, string url)
{
    INFO("Image URL: %s", url.c_str());
    char *tmp_img;
    char tmpname[L_tmpnam];
    tmp_img = tmpnam(tmpname);

    CURL * curl = curl_easy_init();
    bool isOK = false;
    if (curl) {
        FILE * fw = fopen(tmp_img, "wb");
        CURLcode res = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        res = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, fw);
        /*
        res = curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
        res = curl_easy_setopt(curl, CURLOPT_PROXY, "127.0.0.1:7070");
        */

        res = curl_easy_perform(curl);

        if (res == CURLE_OK)
            isOK = true;
        
        fclose(fw);
        curl_easy_cleanup(curl);
    }

    if (isOK) {
        /* call extern application */ 
        char cmd1[100];
        sprintf(cmd1, app.c_str(), tmp_img);
        strcat(cmd1, " &");
        system(cmd1);
        return;
    }
}
#endif

struct ArgsHperlinkClicked {
    string sURL;
    CTelnetView * _this;
};

void CTelnetView::ThreadProcessHyperlinkClicked(gpointer *arg)
{
    string sURL = ((struct ArgsHperlinkClicked *)arg)->sURL;
    CTelnetView * _this = ((struct ArgsHperlinkClicked *)arg)->_this;
    free((struct ArgsHperlinkClicked *)arg);

#ifdef USE_WGET
    if (_this->m_bWgetFiles == true) {
        const char* t_pcURL = sURL.c_str();
        char* t_pcDot = const_cast<char *>(strrchr(t_pcURL, '.')) + 1;
        char t_cFileType = strlen(t_pcURL) - (t_pcDot -t_pcURL);
        if (t_cFileType == 3) {
            if (strncmp(t_pcDot, "rar", 3) == 0 ||
                    strncmp(t_pcDot, "zip", 3) == 0 ||
                    strncmp(t_pcDot, "tgz", 3) == 0 ||
                    strncmp(t_pcDot, "tbz", 3) == 0)
            {
                string t_sURL = sURL;
                t_sURL.insert(0, "wget ");
                t_sURL.append(" &");
                system(t_sURL.c_str());
                return;
            }
        }
    }
#endif
//#if !defined(MOZ_PLUGIN)
    if( 0 == strncmpi( sURL.c_str(), "telnet:", 7) ||
     0 == strncmpi( sURL.c_str(), "ssh:", 4) ||
     0 == strncmpi( sURL.c_str(), "ssh1:", 4) )
    {
        /*
        const char* psURL = sURL.c_str() + 7;
        while( *psURL == '/' )
            ++psURL;
        if( !*psURL )
            return;
        sURL = psURL;
        if( '/' == sURL[sURL.length()-1] )
            sURL = string( sURL.c_str(), 0, sURL.length()-1 );
            */
        gdk_threads_enter();
        _this->m_pParentFrame->NewCon( sURL, sURL );
        gdk_threads_leave();
        return;
    }
//#endif /* !defined(MOZ_PLUGIN) */

    // In URL, the char "&" will be read as "background execution" when run the browser command without " "
    string origURL = sURL;
    sURL.insert(0,"\"");
    sURL.append("\"");

    string app;
    bool isLinkImage = false;
    const char *str_url = sURL.c_str();
    if( !strstr( sURL.c_str(), "://") && strchr(sURL.c_str(), '@'))
    {
        app = _this->m_MailClient;
        if( strncmpi( sURL.c_str(), "mailto:", 7 ) )
            sURL.insert( 0, "mailto:" );
    }
    else if (!strncmpi(str_url + sURL.length() - 4, "com", 3) ||
            !strncmpi(str_url + sURL.length() - 5, "com/", 4) ||
            !strncmpi(str_url + sURL.length() - 4, "net", 3) ||
            !strncmpi(str_url + sURL.length() - 5, "net/", 4) ||
            !strncmpi(str_url + sURL.length() - 3, "org", 3) ||
            !strncmpi(str_url + sURL.length() - 5, "org/", 4) ||
            !strncmpi(str_url + sURL.length() - 5, "html", 4) ||
            !strncmpi(str_url + sURL.length() - 4, "htm", 3)) {
        app = m_WebBrowser;
    }
#ifdef USE_IMAGEVIEW
    else if (!strncmpi(str_url + sURL.length() - 4, "jpg", 3) ||
            !strncmpi(str_url + sURL.length() - 4, "gif", 3) ||
            !strncmpi(str_url + sURL.length() - 4, "bmp", 3) ||
            !strncmpi(str_url + sURL.length() - 4, "png", 3) ||
            isImageURL(origURL.c_str())) {

        isLinkImage = true;

        /*
        gdk_threads_enter();
        _this->m_pParentFrame->ShowDownloadProcessBar();
        gdk_threads_leave();
        */

        thr_downimage(m_ImageViewer, origURL);
        return;
    }
#endif
    else
        app = m_WebBrowser;

    char *cmdline = new char[ app.length() + sURL.length() + 10 ];
    if( strstr(app.c_str(), "%s") )
    {
        sprintf( cmdline, app.c_str(), sURL.c_str() );
    }
    else
    {
        memcpy(cmdline, app.c_str(), app.length());
        cmdline[app.length()] = ' ';
        memcpy( &cmdline[app.length() + 1], sURL.c_str(), sURL.length() + 1);
    }
    strcat(cmdline, " &");	// launch the browser in background.
    if (system(cmdline) == -1)	// Is this portable?
    {
        g_print("Run `%s` failed.\n", cmdline);
    }
    delete []cmdline;
}

void CTelnetView::OnHyperlinkClicked(string sURL)
{
    GThread *tid = NULL;
    struct ArgsHperlinkClicked * args = new (struct ArgsHperlinkClicked);
    args->sURL = sURL;
    args->_this = this;
    tid = g_thread_create((GThreadFunc) &ThreadProcessHyperlinkClicked,
            (gpointer *) args, FALSE, NULL);
}

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
  #pragma implementation "termview.h"
#endif


#include "termview.h"
#include "termdata.h"
#include "termsel.h"
#include "encoding.h"

#include <string>

#include <gdk/gdkx.h>
#include <pango/pangoxft.h>
#include <math.h>
#include "font.h"

using namespace std;

static gboolean on_key_pressed(GtkWidget* wnd UNUSED, GdkEventKey *evt, CTermView* _this)
{
	bool ret = gtk_im_context_filter_keypress(_this->m_IMContext, evt );
	if( !_this->PreKeyDown(evt) && !ret )
		ret = _this->OnKeyDown(evt);
	return ret;
}

static gboolean on_key_released(GtkWidget* wnd UNUSED, GdkEventKey
*evt, CTermView* _this)
{
        return gtk_im_context_filter_keypress(_this->m_IMContext, evt);
}

static void on_im_commit(GtkIMContext *im UNUSED, gchar *arg, CTermView* _this)
{
	_this->OnTextInput(arg);
}

static void on_im_preedit_changed(GtkIMContext *im UNUSED, CTermView* _this)
{
	_this->OnPreedit();
}

static gboolean on_mouse_down(GtkWidget* widget UNUSED, GdkEventButton* evt, CTermView* _this)
{
	switch(evt->button)
	{
	case 1:
		_this->OnLButtonDown(evt);
		break;
	case 2:
		_this->OnMButtonDown(evt);
		break;
	case 3:
		_this->OnRButtonDown(evt);
	default:
		_this->OnButtonDown(evt);
	}
	return true;
}

static gboolean on_mouse_up(GtkWidget* widget UNUSED, GdkEventButton* evt, CTermView* _this)
{
	switch(evt->button)
	{
	case 1:
		_this->OnLButtonUp(evt);
		break;
	case 3:
		_this->OnRButtonUp(evt);
	default:
		_this->OnButtonUp(evt);
	}
	return true;
}

static gboolean on_mouse_move(GtkWidget* widget UNUSED, GdkEventMotion* evt, CTermView* _this)
{
	if (evt->is_hint)
	{
		int x, y;
		GdkModifierType state;
		gdk_window_get_pointer (evt->window, &x, &y, &state);
		evt->x = x;	evt->y = y;
		evt->state = state;
	}
	_this->OnMouseMove(evt);
	return true;
}

static gboolean on_mouse_scroll(GtkWidget* widget UNUSED, GdkEventScroll* evt, CTermView* _this)
{
	_this->OnMouseScroll(evt);
	return true;
}

void CTermView::OnBeforeDestroy( GtkWidget* widget UNUSED, CTermView* _this)
{
	XftDrawDestroy( _this->m_XftDraw);
	_this->m_XftDraw = NULL;

        /*
        cairo_destroy( _this->m_CairoRender );
        _this->m_CairoRender = NULL;
        */

	DEBUG("unrealize, destroy XftDraw");
}

GdkCursor* CTermView::m_HandCursor = NULL;
GdkCursor* CTermView::m_ExitCursor = NULL;
GdkCursor* CTermView::m_BullsEyeCursor = NULL;
GdkCursor* CTermView::m_PageDownCursor = NULL;
GdkCursor* CTermView::m_PageUpCursor = NULL;
GdkCursor* CTermView::m_EndCursor = NULL;
GdkCursor* CTermView::m_HomeCursor = NULL;
int CTermView::m_CursorState = 0;

CTermView::CTermView()
        : CView(), m_pColorTable(CTermCharAttr::GetDefaultColorTable())
{
	m_pTermData = NULL;
	m_GC = NULL;
	m_ShowBlink = false;
	m_Font = NULL;
	m_FontEn = NULL;
        m_FontNow = NULL;
        m_FontEnNow = NULL;
	m_XftDraw = NULL;
        //m_CairoRender = NULL;
	m_CharW = 18;
	m_CharH = 18;
	m_LeftMargin = 0;
	m_TopMargin = 0;
	m_bHorizontalCenterAlign = false;
	m_bVerticalCenterAlign = false;

        m_TermWidth = 1024;
        m_TermHeight = 616;
        m_TermWidthBaseLine = 1024;
        m_TermHeightBaseLine = 616;
        m_TermAspectRatio = 1.66;
	
	m_CancelSel = false;

	gtk_widget_add_events(m_Widget, GDK_EXPOSURE_MASK
		 | GDK_KEY_PRESS_MASK
		 | GDK_BUTTON_PRESS_MASK
		 | GDK_BUTTON_MOTION_MASK
		 | GDK_BUTTON_RELEASE_MASK
		 | GDK_POINTER_MOTION_MASK
		 | GDK_POINTER_MOTION_HINT_MASK
		 | GDK_ALL_EVENTS_MASK);

	GTK_WIDGET_SET_FLAGS(m_Widget, GTK_CAN_FOCUS);
	gtk_widget_set_double_buffered(m_Widget, false);

	g_signal_connect(G_OBJECT(m_Widget), "unrealize", G_CALLBACK(CTermView::OnBeforeDestroy), this);

	g_signal_connect(G_OBJECT(m_Widget), "key_press_event", G_CALLBACK(on_key_pressed), this);

        g_signal_connect(G_OBJECT(m_Widget), "key_release_event", G_CALLBACK(on_key_released), this);

        g_signal_connect(G_OBJECT(m_Widget), "button_press_event", G_CALLBACK(on_mouse_down), this);

	g_signal_connect(G_OBJECT(m_Widget), "button_release_event", G_CALLBACK(on_mouse_up), this);

	g_signal_connect(G_OBJECT(m_Widget), "motion_notify_event", G_CALLBACK(on_mouse_move), this);

	g_signal_connect(G_OBJECT(m_Widget), "scroll_event", G_CALLBACK(on_mouse_scroll), this);

	m_CharPaddingX = m_CharPaddingY = 0;
	m_AutoFontSize = true;
	m_pHyperLinkColor = NULL;

	m_IMContext = gtk_im_multicontext_new();
//	gtk_im_context_set_use_preedit( m_IMContext, FALSE );
	g_signal_connect( G_OBJECT(m_IMContext), "commit", G_CALLBACK(on_im_commit), this );
	g_signal_connect( G_OBJECT(m_IMContext), "preedit-changed", G_CALLBACK(on_im_preedit_changed), this );

	if( m_HandCursor )
		gdk_cursor_ref(m_HandCursor);
	else
		m_HandCursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_HAND2);
	if( m_ExitCursor )
	  gdk_cursor_ref(m_ExitCursor);
	else
	  m_ExitCursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_SB_LEFT_ARROW);
	if( m_BullsEyeCursor )
	  gdk_cursor_ref(m_BullsEyeCursor);
	else
	  m_BullsEyeCursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_SB_RIGHT_ARROW);
	if( m_PageUpCursor )
	  gdk_cursor_ref(m_PageUpCursor);
	else
	  m_PageUpCursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_SB_UP_ARROW);
	if( m_PageDownCursor )
	  gdk_cursor_ref(m_PageDownCursor);
	else
	  m_PageDownCursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_SB_DOWN_ARROW);
	if( m_EndCursor )
	  gdk_cursor_ref(m_EndCursor);
	else
	  m_EndCursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_BOTTOM_SIDE);
	if( m_HomeCursor )
	  gdk_cursor_ref(m_HomeCursor);
	else
	  m_HomeCursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_TOP_SIDE);

}


void CTermView::OnPaint(GdkEventExpose* evt)
{
	// Hide the caret to prevent drawing problems.
	m_Caret.Hide();

	GdkDrawable* dc = m_Widget->window;
	if(!GDK_IS_DRAWABLE(dc))
	{
		DEBUG("WARNNING! Draw on DELETED widget!");
		return;
	}

	int w = m_Widget->allocation.width, h = m_Widget->allocation.height;

	if( m_pTermData )
	{
		// Only redraw the invalid area to greatly enhance performance.
		int top = evt->area.y;		int bottom = top + evt->area.height;
		int left = evt->area.x;		int right = left + evt->area.width;
		this->PointToLineCol( &left, &top );
		this->PointToLineCol( &right, &bottom );

		if(right < m_pTermData->m_ColsPerPage)	right++;
		if(bottom < m_pTermData->m_RowsPerPage)	bottom++;
		if(top > 0)	top-=top>1?2:1;

		for( int row = top; row < bottom; row++ )
		{
			for( int col = left; col < right; )
				col += DrawChar( row, col );
		}

		gdk_gc_set_rgb_fg_color(m_GC, CTermCharAttr::GetDefaultColorTable(0));
		left = m_pTermData->m_ColsPerPage*m_CharW-2;

		/* repaint some region that should be repainted */
		gdk_draw_rectangle(dc, m_GC, true, 0, 0, m_LeftMargin, h ); // fill left margin
		gdk_draw_rectangle(dc, m_GC, true, left + m_LeftMargin, 0, w-left, h ); // fill right marin

		top = m_pTermData->m_RowsPerPage*m_CharH;
		gdk_draw_rectangle(dc, m_GC, true, 0, 0, w, m_TopMargin ); // fill top margin
		gdk_draw_rectangle(dc, m_GC, true, 0, top + m_TopMargin, w, h - top ); // fill bottom margin

		m_Caret.Show();
	}
	else
	{
		gdk_gc_set_rgb_bg_color(m_GC, CTermCharAttr::GetDefaultColorTable(0));
		gdk_draw_rectangle(dc, m_GC, true, 0, 0, w, h );
	}

}

void CTermView::OnSetFocus(GdkEventFocus* evt UNUSED)
{
	gtk_im_context_focus_in(m_IMContext);
}


bool CTermView::OnKeyDown(GdkEventKey* evt UNUSED)
{
	return true;
}


void CTermView::OnTextInput(const gchar* string UNUSED)
{
    // Override this function to process text input.
}

void CTermView::OnPreedit()
{
	// Override this function to process preedit info
}

void CTermView::OnCreate()
{
	CWidget::OnCreate();
	gtk_im_context_set_client_window(m_IMContext, m_Widget->window);

	m_XftDraw = XftDrawCreate(
		GDK_WINDOW_XDISPLAY(m_Widget->window),
		GDK_WINDOW_XWINDOW(m_Widget->window),
		GDK_VISUAL_XVISUAL(gdk_drawable_get_visual(m_Widget->window)),
		GDK_COLORMAP_XCOLORMAP (gdk_drawable_get_colormap(m_Widget->window)));
	XftDrawSetSubwindowMode(m_XftDraw, IncludeInferiors);

        /*
        m_CairoRender = gdk_cairo_create( m_Widget->window );
        */

	if( !m_Font )
		m_Font = new CFont("Sans", 16);
	if( !m_FontEn )
		m_FontEn = new CFont("Sans", 16);

	m_GC = gdk_gc_new(m_Widget->window);
	gdk_gc_copy(m_GC, m_Widget->style->black_gc);

	m_Caret.Create(this, m_GC);
	m_Caret.Show();
}

// gtk+ 2.8.0 to 2.10.13 all have serious bug in gdk_draw_trapezoids()
// So version check is needed here.
#if GTK_CHECK_VERSION( 2, 10, 13 ) ||  ! GTK_CHECK_VERSION( 2, 8, 0 )
#else
#define	HAVE_GDK_DRAW_TRAPEZOIDS_BUG

// This is the correct implementation of gdk_draw_trapezoids taken from gtk+ 2.10.14
static void fixed_gdk_draw_trapezoids (GdkDrawable *drawable, 
						GdkGC *gc, GdkTrapezoid   *trapezoids,
						gint n_trapezoids, GdkColor* color, GdkRectangle* clip_rect )
{
  cairo_t *cr;
  int i;

  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (n_trapezoids == 0 || trapezoids != NULL);

  cr = gdk_cairo_create (drawable);

  // color
  gdk_cairo_set_source_color (cr, color);

  //clip
  cairo_reset_clip (cr);
  if (clip_rect)
    {
      cairo_save (cr);

      cairo_identity_matrix (cr);
      cairo_translate (cr, gc->clip_x_origin, gc->clip_y_origin);

      cairo_new_path (cr);
      gdk_cairo_rectangle (cr, clip_rect);

      cairo_restore (cr);

      cairo_clip (cr);
    }
 
  for (i = 0; i < n_trapezoids; i++)
    {
      cairo_move_to (cr, trapezoids[i].x11, trapezoids[i].y1);
      cairo_line_to (cr, trapezoids[i].x21, trapezoids[i].y1);
      cairo_line_to (cr, trapezoids[i].x22, trapezoids[i].y2);
      cairo_line_to (cr, trapezoids[i].x12, trapezoids[i].y2);
      cairo_close_path (cr);
    }
  cairo_fill (cr);
  cairo_destroy (cr);
}
#endif

bool CTermView::DrawSpaceFillingChar(const char* ch, int len UNUSED, int x, int y, GdkRectangle* clip UNUSED, GdkColor* clr UNUSED)
{
	GdkDrawable* dc = m_Widget->window;
	guchar* uchar = (guchar*)ch;
// NOTE: Omit this check to increase performance.
// IsSpaceFillingChar should be called prior to calling this method.
//	if( len >= 3 && (int)uchar[0] == 0xe2 )
	{
//		gdk_gc_set_rgb_fg_color( m_GC, clr );
		switch( uchar[1] )
		{
		case 0x96:
			if( uchar[2] >= 0x81 && uchar[2] <= 0x88 )
			{
				int h = m_CharH * (uchar[2] - 0x80) / 8;
				gdk_draw_rectangle( dc, m_GC, true, x , y + m_CharH - h, m_CharW * 2, h );
			}
			else if( uchar[2] >= 0x89 && uchar[2] <= 0x8f )
			{
				// FIXME: There are still some potential bugs here.
				// See the welcome screen of telnet://ptt.cc for example
				int w = m_CharW * 2 * (8 - (uchar[2] - 0x88)) / 8;
				gdk_draw_rectangle( dc, m_GC, true, x, y, w, m_CharH );
			}
/*
			else if( uchar[2] == 0xa0 )
			{
				gdk_draw_rectangle( dc, m_GC, true, rc->x, rc->y, m_CharW * 2, m_CharH );
			}
			else if( uchar[2] == 0xa1 )
			{
				int w = m_CharW * 2 * (uchar[2] - 0x88) / 8;
				gdk_draw_rectangle( dc, m_GC, false, rc->x, rc->y, w, m_CharH );
			}
*/			else
				return false;
			return true;
		case 0x95:
			return false;
		case 0x94:
			return false;
		case 0x97:
			{
				GdkTrapezoid tz;

				tz.y1 = y;
				tz.y2 = y + m_CharH;
				tz.x11 = tz.x12 = x;
				tz.x21 =	tz.x22 = x + m_CharW * 2;

				switch( uchar[2] )
				{
				case 0xa2:
					tz.x11 = tz.x21;
					break;
				case 0xa3:
					tz.x21 = tz.x11;
					break;
				case 0xa4:
					tz.x22 = tz.x12;
					break;
				case 0xa5:
					tz.x12 = tz.x22;
					break;
				default:
					return false;
				}

// workarounds for serious bug in gtk+ 2.8.0 to 2.10.12
#ifdef HAVE_GDK_DRAW_TRAPEZOIDS_BUG
				fixed_gdk_draw_trapezoids( dc, m_GC, &tz, 1, clr, clip );
#else
				gdk_draw_trapezoids( dc, m_GC, &tz, 1 );
#endif
				return true;
			}
		}
	}
	return false;
}

void CTermView::DrawCharAt(gunichar ch, int row, int col, int cwid)
{
	GdkDrawable* dc = m_Widget->window;
	if(!GDK_IS_DRAWABLE(dc) && m_XftDraw == NULL)
	{
		//		g_warning("Draw on DELETED widget!\n");
		return;
	}

	const char* pLine = m_pTermData->m_Screen[m_pTermData->m_FirstLine + row];
	CTermCharAttr* pAttr = m_pTermData->GetLineAttr(pLine);
//	pLine += col;
	pAttr += col;

	int left = m_CharW * col + m_LeftMargin;
	int top = m_CharH * row + m_TopMargin;
	int bgw = m_CharW * cwid;

	GdkColor iFg, iBg;

	GdkColor* Fg = pAttr[0].GetFgColor( m_pColorTable );
	GdkColor* Bg = pAttr[0].GetBgColor( m_pColorTable );
	// if it's property is inverse, GetFgColor & GetBgColor will swap Bg & Fg for us.

	XftColor xftclr;
	xftclr.pixel = 0;
	xftclr.color.red = Fg->red;
	xftclr.color.green = Fg->green;
	xftclr.color.blue = Fg->blue;
	xftclr.color.alpha = 0xffff;

	// set clip mask
	GdkRectangle rect;
	XRectangle xrect;
	xrect.x = rect.x = left;
	xrect.y = rect.y = top;
	xrect.width = rect.width = m_CharW * cwid;
	xrect.height = rect.height = m_CharH;
	gdk_gc_set_clip_origin( m_GC, 0, 0 );
	gdk_gc_set_clip_rectangle( m_GC, &rect );
	Region xregion = XCreateRegion();
	XUnionRectWithRegion( &xrect, xregion, xregion );
	XftDrawSetClip( m_XftDraw, xregion );
	XDestroyRegion( xregion );

	gdk_gc_set_rgb_fg_color( m_GC, Bg );
	gdk_draw_rectangle( dc, m_GC, true, left , top, bgw, m_CharH );

	gdk_gc_set_rgb_fg_color( m_GC, Fg );

//	if( !pAttr[0].IsBlink() || m_ShowBlink )	// If text should be drawn.
//	{
	if( ' ' != ch  && '\0' != ch )
	{
		gchar utf8_ch[10];
		gint len = g_unichar_to_utf8(ch, utf8_ch);
		if( len )
		{
			XftFont* font;
			if (isascii (utf8_ch[0])) {
				font = m_FontEnNow->GetXftFont();
			} else {
				font = m_FontNow->GetXftFont();
			}

			if( !IsSpaceFillingChar(utf8_ch, len) || !DrawSpaceFillingChar( utf8_ch, len, left, top, &rect, Fg ) )
				XftDrawStringUtf8( m_XftDraw, &xftclr, font, left, top + font->ascent, (FcChar8*)utf8_ch, len );
		}
	}
	int y = top + m_CharH - 1;
	gdk_draw_line( dc, m_GC, left, y, left + bgw - 1, y );
//	}
	gdk_gc_set_clip_rectangle( m_GC, NULL );
	XftDrawSetClip( m_XftDraw, NULL );
}

int CTermView::DrawChar(int row, int col)
{
    	GdkDrawable* dc = m_Widget->window;
	if(!GDK_IS_DRAWABLE(dc) && m_XftDraw == NULL)
	{
//		g_warning("Draw on DELETED widget!\n");
		return 1;
	}

	const char* pLine = m_pTermData->m_Screen[m_pTermData->m_FirstLine + row];
	CTermCharAttr* pAttr = m_pTermData->GetLineAttr(pLine);
	int w = 2;
	bool is_mbcs2 = false;
	switch( pAttr[col].GetCharSet() )
	{
	case CTermCharAttr::CS_MBCS1:
		break;
	case CTermCharAttr::CS_MBCS2:
		col--;
		is_mbcs2 = true;
//		This will not cause any problem at any time because 'col' always > 0.
//		In CTermData_this->DetectCharSets() I've done some checks to ensure that the first
//		character of every lines cannot be marked as second bytes of MBCS characters.
		break;
	default:
//	case CTermCharAttr::CS_ASCII:
		w = 1;
	}
	pLine += col;
	pAttr += col;

	bool bSel[2];
	bSel[0] = m_pTermData->m_Sel->Has( row, col );
	if ( w > 1 )
		bSel[1] = m_pTermData->m_Sel->Has( row, col + 1 );

	int left = m_CharW * col + m_LeftMargin;
	int top = m_CharH * row + m_TopMargin;
	int bgw = m_CharW * w;

	GdkColor iFg, iBg;

	for( int i = 0; i < w; i++ )	//	do the drawing
	{
		GdkColor* Fg = pAttr[i].GetFgColor( m_pColorTable );
		GdkColor* Bg = pAttr[i].GetBgColor( m_pColorTable );
		// if it's property is inverse, GetFgColor & GetBgColor will swap Bg & Fg for us.

		if( bSel[i] )	// if it's selected, reverse two colors.
		{
			iFg.red = ~Fg->red;
			iFg.green = ~Fg->green;
			iFg.blue = ~Fg->blue;
			Fg = &iFg;
			iBg.red = ~Bg->red;
			iBg.green = ~Bg->green;
			iBg.blue = ~Bg->blue;
			Bg = &iBg;
		}

		XftColor xftclr;
		xftclr.pixel = 0;
		xftclr.color.red = Fg->red;
		xftclr.color.green = Fg->green;
		xftclr.color.blue = Fg->blue;
		xftclr.color.alpha = 0xffff;

		// set clip mask
		GdkRectangle rect;
		XRectangle xrect;
		xrect.x = rect.x = left + m_CharW * i;
		xrect.y = rect.y = top;
		xrect.width = rect.width = m_CharW * ( w - i );
		xrect.height = rect.height = m_CharH;
		gdk_gc_set_clip_origin( m_GC, 0, 0 );
		gdk_gc_set_clip_rectangle( m_GC, &rect );
		Region xregion = XCreateRegion();
		XUnionRectWithRegion( &xrect, xregion, xregion );
		XftDrawSetClip( m_XftDraw, xregion );
		XDestroyRegion( xregion );

		gdk_gc_set_rgb_fg_color( m_GC, Bg );
		gdk_draw_rectangle( dc, m_GC, true, left , top, bgw, m_CharH );

		gdk_gc_set_rgb_fg_color( m_GC, Fg );

		if( !pAttr[i].IsBlink() || m_ShowBlink )	// If text should be drawn.
		{
			if( ' ' != *pLine && '\0' != *pLine )
			{
				gsize wl;
				gchar *utf8_ch = my_convert( pLine, w, "UTF-8", m_pTermData->m_Encoding.c_str(), &wl);
				if( utf8_ch )
				{
					XftFont* font;
					if (isascii (utf8_ch[0])) {
						font = m_FontEnNow->GetXftFont();
					} else {
						font = m_FontNow->GetXftFont();
					}

					if( !IsSpaceFillingChar(utf8_ch, wl) || !DrawSpaceFillingChar( utf8_ch, wl, left, top, &rect, Fg ) )
						XftDrawStringUtf8( m_XftDraw, &xftclr, font, left, top + font->ascent, (FcChar8*)utf8_ch, wl );
					g_free(utf8_ch);
				}
			}
			if( pAttr[i].IsUnderLine() )
			{
				int y = top + m_CharH - 1;
				gdk_draw_line( dc, m_GC, left, y, left + bgw - 1, y );
			}
		}
		// 2004.08.07 Added by PCMan: Draw the underline of hyperlinks.
		if( pAttr[i].IsHyperLink() ) 
		{
//			dc.SetPen(wxPen(m_HyperLinkColor, 1, wxSOLID));
//			int bottom = top + m_CharH - 1;
//			dc.DrawLine(left, bottom, left+bgw, bottom);

			if(m_pHyperLinkColor)
	 			gdk_gc_set_rgb_fg_color( m_GC, m_pHyperLinkColor );
			int y = top + m_CharH - 1;
			gdk_draw_line( dc, m_GC, left, y, left + bgw - 1, y );
		}

		// two cells have the same attributes
		if( i == 0 && pAttr[0].IsSameAttr( pAttr[1].AsShort() ) && bSel[0] == bSel[1] )
			break;
	}
	gdk_gc_set_clip_rectangle( m_GC, NULL );
	XftDrawSetClip( m_XftDraw, NULL );

	return is_mbcs2 ? 1 : w;
}

void CTermView::PointToLineCol(int *x, int *y, bool *left)
{
	*x -= m_LeftMargin;

	int pos = *x % m_CharW;

	*x /= m_CharW;
	if (*x < 0)
	{
		*x = 0;
		pos = 0; // so that *left = true
	}
	else if( *x >= m_pTermData->m_ColsPerPage )
	{
		*x = m_pTermData->m_ColsPerPage - 1;
		pos = m_CharW; // so taht *left = false
	}

	*y -= m_TopMargin;
	*y /= m_CharH;
	if(*y <0 )
		*y = 0;
	else if( *y >= m_pTermData->m_RowsPerPage )
		*y = m_pTermData->m_RowsPerPage-1;

	if ( left )
	{
		const char* pLine = m_pTermData->m_Screen[m_pTermData->m_FirstLine + *y];
		CTermCharAttr* pAttr = m_pTermData->GetLineAttr( pLine );

		switch( pAttr[*x].GetCharSet() )
		{
		case CTermCharAttr::CS_MBCS1:
			*left = true;
			break;
		case CTermCharAttr::CS_MBCS2:
			*left = false;
			break;
		default:
			*left = pos < ( m_CharW + 1 ) / 2;
			break;
		}
	}
}

void CTermView::OnSize(GdkEventConfigure* evt UNUSED)
{
    if( !m_pTermData )
        return;

    int w, h;
    w = m_Widget->allocation.width;
    h = m_Widget->allocation.height;
    if (w / m_TermAspectRatio > h)
        w = (int)floor(h * m_TermAspectRatio);
    else
        h = (int)floor(w / m_TermAspectRatio);

    m_CharW = w / m_pTermData->m_ColsPerPage;
    m_CharH = h / m_pTermData->m_RowsPerPage;

    m_TermWidth = m_CharW * m_pTermData->m_ColsPerPage;
    m_TermHeight = m_CharH * m_pTermData->m_RowsPerPage;

    INFO("Cell: width=%d, height=%d", m_CharW, m_CharH);
    INFO("Term: width=%d, height=%d", m_TermWidth, m_TermHeight);

    RecalcCharSize();

    if( m_bHorizontalCenterAlign )
        m_LeftMargin = (m_Widget->allocation.width - m_CharW * m_pTermData->m_ColsPerPage ) / 2;
    else
        m_LeftMargin = 0;

    if( m_bVerticalCenterAlign )
        m_TopMargin = (m_Widget->allocation.height - m_CharH * m_pTermData->m_RowsPerPage ) / 2;
    else
        m_TopMargin = 0;

    m_Caret.SetSize(m_CharW, 2);
    UpdateCaretPos();
    m_Caret.Show();
}

static int DrawCharWrapper( int row, int col, void *data )
{
	CTermView *tv = (CTermView *) data;

	return tv->DrawChar( row, col );
}

void CTermView::ClearSelection()
{
	m_CancelSel = true;

	m_Caret.Hide();
	m_pTermData->m_Sel->Unselect( DrawCharWrapper, this );
	m_Caret.Show( false );
}

void CTermView::ExtendSelection( int row, int col, bool left UNUSED )
{
	row += m_pTermData->m_FirstLine;

	const char* pLine = m_pTermData->m_Screen[row];
	CTermCharAttr* pAttr = m_pTermData->GetLineAttr( pLine );

	if( pAttr[col].GetCharSet() == CTermCharAttr::CS_MBCS2 )
		col--;

	int klass = m_pTermData->GetCharClass( row, col );
	int i;

	/* decide start */
	for( i = col - 1; i >= 0; i-- )
	{
		int w = 1;
		if( pAttr[col].GetCharSet() == CTermCharAttr::CS_MBCS2 )
		{
			i--;
			w++;
		}

		if( m_pTermData->GetCharClass( row, i ) != klass )
		{
			i += w;
			break;
		}
	}
	if( i < 0 )
		i = 0;

	m_pTermData->m_Sel->NewStart( row, i, true );

	/* decide end */
	for( i = col + 1; i < m_pTermData->m_ColsPerPage; i++ )
	{
		int w = 1;
		if( pAttr[col].GetCharSet() == CTermCharAttr::CS_MBCS2 )
		{
			i++;
			w++;
		}

		if( m_pTermData->GetCharClass( row, i ) != klass )
		{
			i -= w;
			break;
		}
	}
	if( i >= m_pTermData->m_ColsPerPage )
		i = m_pTermData->m_ColsPerPage - 1;

	m_pTermData->m_Sel->ChangeEnd( row, i, false, DrawCharWrapper, this );
}

void CTermView::OnLButtonDown(GdkEventButton* evt)
{
	SetFocus();
	m_CancelSel = false;

	if( !m_pTermData )
		return;

	int x = (int)evt->x;
	int y = (int)evt->y;
	bool left;

	PointToLineCol( &x, &y, &left );

	if( evt->type == GDK_3BUTTON_PRESS )
	{
		m_pTermData->m_Sel->NewStart( y, 0, true );
		m_pTermData->m_Sel->ChangeEnd( y, m_pTermData->m_ColsPerPage - 1,
				               false, DrawCharWrapper, this );
	}
	else if( evt->type == GDK_2BUTTON_PRESS )
		ExtendSelection( y, x, left );
	else
	{
		// clear the old selection
		if( !m_pTermData->m_Sel->Empty() )
			ClearSelection();

		SetCapture();

		INFO("x=%d, y=%d, grab=%d", x, y, HasCapture());

		m_pTermData->m_Sel->NewStart( y, x, left,
				(evt->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_CONTROL_MASK)) );
	}
}


void CTermView::OnMouseMove(GdkEventMotion* evt UNUSED)
{

}


void CTermView::OnMouseScroll(GdkEventScroll* evt UNUSED)
{

}

void CTermView::OnRButtonDown(GdkEventButton* evt UNUSED)
{

}

void CTermView::OnLButtonUp(GdkEventButton* evt)
{
	if( !m_pTermData )
		return;
	ReleaseCapture();

	m_pTermData->m_Sel->Canonicalize();

	//	2004.08.07 Added by PCMan.  Hyperlink detection.
	//	If no text is selected, consider hyperlink.
	if( m_pTermData->m_Sel->Empty() )
	{
		int x = (int)evt->x;
		int y = (int)evt->y; 
		PointToLineCol( &x, &y );

		int start, end;
                char buf[4096];
                if (HyperLinkHitTest(x, y, buf)) {
			char* pline = m_pTermData->m_Screen[y];
			OnHyperlinkClicked( string( (buf) ) );
		}
	}
	else	// if there is a selected area
	{
		CopyToClipboard(true, false, false);
	}

}

void CTermView::OnRButtonUp(GdkEventButton* evt UNUSED)
{
}

void CTermView::OnKillFocus(GdkEventFocus *evt UNUSED)
{
	gtk_im_context_focus_out(m_IMContext);
}

// Preview the GdkEventKey before it's sent to OnKeyDown
// regardless of the returned value of im_context_filter_key_press.
bool CTermView::PreKeyDown(GdkEventKey *evt UNUSED)
{
	return false;
}

void CTermView::OnBlinkTimer()
{
	m_ShowBlink = !m_ShowBlink;
	if(m_pTermData)
	{
//		const int left = 0;	const int right = m_pTermData->m_ColsPerPage;
		for( int row = 0; row < m_pTermData->m_RowsPerPage; row++ )
		{
//			DrawLine( dc, i, y, true , left, right );
			CTermCharAttr* attr = m_pTermData->GetLineAttr(
					m_pTermData->m_Screen[m_pTermData->m_FirstLine + row] );

			for( int col = 0; col < m_pTermData->m_ColsPerPage; )
			{
				if( attr[col].IsBlink())
					col += DrawChar( row, col );
				else
					col++;
			}
		}
	}
	m_Caret.Blink();
}

void CTermView::OnMButtonDown(GdkEventButton* evt UNUSED)
{
	PasteFromClipboard(true, false);
}

void CTermView::OnButtonDown(GdkEventButton *evt UNUSED)
{
}

void CTermView::OnButtonUp(GdkEventButton *evt UNUSED)
{
}

vector<CTermCharAttr> CTermView::m_s_ANSIColorAttr;

void CTermView::PasteFromClipboard(bool primary, bool with_color)
{
	string text;
	if(!with_color || m_s_ANSIColorAttr.empty())
	{
		GtkClipboard* clipboard = gtk_clipboard_get( primary ? 
                        GDK_SELECTION_PRIMARY : GDK_NONE);
		INFO("paste");
		gchar* utext = gtk_clipboard_wait_for_text(clipboard);
		if( !utext )
			return;
		INFO("%s", utext);

		DoPasteFromClipboard( string(utext), false);
		g_free(utext);
	}
	else {
		GtkClipboard* clipboard = gtk_clipboard_get( primary ? 
                        GDK_SELECTION_PRIMARY : GDK_NONE);
		INFO("paste");
		gchar* utext = gtk_clipboard_wait_for_text(clipboard);
		if( !utext )
			return;
		INFO("%s", utext);
		DoPasteFromClipboard( string(utext), true);
        }
}

void CTermView::DoPasteFromClipboard(string text UNUSED, bool contain_ansi_color UNUSED)
{

}

void CTermView::CopyToClipboard(bool primary, bool with_color, bool trim)
{
	string text;
	if(!m_pTermData)
		return;

        /* Clear the color infomation */
        m_s_ANSIColorAttr.clear();


	if( with_color ) {
            text = m_pTermData->GetSelectedTextWithAttr(&m_s_ANSIColorAttr, trim);
            gsize wl = 0;
            const gchar * utext = my_convert(text.c_str(), text.length(), 
                    "UTF-8", m_pTermData->m_Encoding.c_str(), &wl);

            if(!utext)
                return;

            GtkClipboard* clipboard = gtk_clipboard_get(  primary ? 
                    GDK_SELECTION_PRIMARY : GDK_NONE );
            gtk_clipboard_set_text(clipboard, utext, wl );
            g_free((void*)utext);
            INFO("copy(with color): %s", text.c_str());
        }
        else {
            /* Copy the text to Clipboard */
            text = m_pTermData->GetSelectedText(trim);
            INFO("orignal text: %s", text.c_str());
            gsize wl = 0;
            const gchar * utext = my_convert(text.c_str(), text.length(), 
                    "UTF-8", m_pTermData->m_Encoding.c_str(), &wl);

            if(!utext)
                return;

            GtkClipboard* clipboard = gtk_clipboard_get(  primary ? 
                    GDK_SELECTION_PRIMARY : GDK_NONE );
            gtk_clipboard_set_text(clipboard, utext, wl );
        g_free((void*)utext);
            INFO("copy(without color): %s", utext);
	}
}

void CTermView::GetCellSize( int &w, int &h )
{
	if( !m_pTermData->m_ColsPerPage || !m_pTermData->m_RowsPerPage )
	{
		w = 0;
		h = 0;

		return;
	}

	w = ( m_Widget->allocation.width / m_pTermData->m_ColsPerPage ) - m_CharPaddingX;
	h = ( m_Widget->allocation.height / m_pTermData->m_RowsPerPage ) - m_CharPaddingY;
}

void CTermView::SetFont( CFont* font )
{
	if( !font )
            return;

	if( m_Font )
            delete m_Font;

        m_Font = font;

	//RecalcCharSize();
}

void CTermView::SetFontEn( CFont* font )
{
	if( !font )
		return;

	if( m_FontEn )
		delete m_FontEn;

        m_FontEn = font;

	//RecalcCharSize();
}

void CTermView::SetFont( string name, int pt_size, bool anti_alias )
{
	if( m_Font )
		delete m_Font;

        m_Font = new CFont( name, pt_size, anti_alias );

	//RecalcCharSize();
}

void CTermView::SetFontEn( string name, int pt_size, bool anti_alias )
{
	if( m_FontEn )
		delete m_FontEn;

        m_FontEn = new CFont( name, pt_size, anti_alias );

        //RecalcCharSize();
}

void CTermView::SetFontFamily( string name )
{
    m_Font->SetFontFamily( name );

    //RecalcCharSize();
}

void CTermView::SetFontFamilyEn( string name )
{
    m_FontEn->SetFontFamily( name );

    //RecalcCharSize();
}

void CTermView::SetHorizontalCenterAlign( bool is_hcenter )
{
	if( m_bHorizontalCenterAlign == is_hcenter || !m_pTermData )
		return;

	if( (m_bHorizontalCenterAlign = is_hcenter) && GTK_WIDGET_REALIZED(m_Widget) )
		m_LeftMargin = (m_Widget->allocation.width - m_CharW * m_pTermData->m_ColsPerPage ) / 2 ;
	else
		m_LeftMargin = 0;

	if( IsVisible() )
		Refresh();
	UpdateCaretPos();
}

void CTermView::SetVerticalCenterAlign( bool is_vcenter )
{
	if( m_bVerticalCenterAlign == is_vcenter || !m_pTermData )
		return;

	if( (m_bVerticalCenterAlign = is_vcenter) && GTK_WIDGET_REALIZED(m_Widget) )
		m_TopMargin = (m_Widget->allocation.height - m_CharH * m_pTermData->m_RowsPerPage ) / 2 ;
	else
		m_TopMargin = 0;

	if( IsVisible() )
		Refresh();
	UpdateCaretPos();
}

void CTermView::UpdateCaretPos()
{
	if( !m_pTermData )
		return;

	int x = m_pTermData->m_CaretPos.x * m_CharW + m_LeftMargin;
	int y = (m_pTermData->m_CaretPos.y + 1) * m_CharH - 2 + m_TopMargin;
	m_Caret.Move( x, y );

	GdkRectangle rc;
	rc.x = x;	rc.y = y; 	rc.width = 0;	rc.height = 0;
	gtk_im_context_set_cursor_location(m_IMContext, &rc);
}


bool CTermView::HyperLinkHitTest(int x, int y, char * buf)
{
    int colsPerPage = m_pTermData->m_ColsPerPage;
    int i = y * colsPerPage + x;
    int first_position = m_pTermData->m_FirstLine * colsPerPage;
    int last_position = (m_pTermData->m_FirstLine + m_pTermData->m_RowsPerPage) 
        * colsPerPage;
    if (!(i >= first_position && i < last_position &&
            m_pTermData->GetLineAttr(i/colsPerPage)[i%colsPerPage].IsHyperLink()))
        return false;
    int _start, _end;
    for (_start = i; _start > first_position &&
            m_pTermData->GetLineAttr(_start/colsPerPage)[_start%colsPerPage]
            .IsHyperLink(); _start--) ;
    if (!m_pTermData->GetLineAttr(_start/colsPerPage)[_start%colsPerPage]
            .IsHyperLink())
        _start++;
    for( _end = i; _end < last_position &&
            m_pTermData->GetLineAttr(_end/colsPerPage)[_end%colsPerPage]
            .IsHyperLink(); _end++) ;
    int firstline = _start/colsPerPage;
    int endline = _end/colsPerPage;
    int size = 0;
    if (endline > firstline) {
        strncpy(buf+size, m_pTermData->m_Screen[firstline]+_start%colsPerPage,
                colsPerPage - _start%colsPerPage);
        size += colsPerPage - _start%colsPerPage;
    } else {
        strncpy(buf+size, m_pTermData->m_Screen[firstline]+_start%colsPerPage,
                _end - _start);
        size += _end - _start;
    }
    for (int j = firstline + 1; j < endline; j++) {
        strncpy(buf+size, m_pTermData->m_Screen[j], colsPerPage);
        size += colsPerPage;
    }
    if (endline > firstline) {
        strncpy(buf+size, m_pTermData->m_Screen[endline], _end%colsPerPage);
        size += _end%colsPerPage;
    }
    *(buf+size) = '\0';
    return true;
}

/*string CTermView::DownLoadArticle()
{
	const char* pline = m_pTermData->m_Screen[23];
	string result (pline);
	return result;
}*/


void CTermView::OnDestroy()
{
	if( m_Font )
		delete m_Font;
	if( m_FontEn )
		delete m_FontEn;
	if( m_pTermData )
		m_pTermData->m_pView = NULL;

	if( m_HandCursor )
		gdk_cursor_unref(m_HandCursor);
	if( m_HandCursor->ref_count <= 0 )
		m_HandCursor = NULL;

	CView::OnDestroy();	// Remember to destruct parent
}

void CTermView::RecalcCharSize()
{
    m_ScaleRatio = ((double)m_TermWidth)/m_TermWidthBaseLine;
    INFO("ScaleRatio: %lf", m_ScaleRatio);
    INFO("Term: width=%d, height=%d", m_TermWidth, m_TermHeight);

    if ( m_Font ) {
        if ( m_FontNow )
            delete m_FontNow;
        m_FontNow = new CFont( m_Font->GetName(), 
                round(m_Font->GetFontSize() * m_ScaleRatio),
                m_Font->GetAntiAlias());
        INFO("New Font Size: %d", (int)m_FontNow->GetFontSize());
    }

    if ( m_FontEn ) {
        if ( m_FontEnNow )
            delete m_FontEnNow;
        m_FontEnNow = new CFont( m_FontEn->GetName(), 
                round(m_FontEn->GetFontSize() * m_ScaleRatio),
                m_FontEn->GetAntiAlias());
        INFO("New FontEn Size: %d", (int)m_FontEnNow->GetFontSize());
    }
}

void CTermView::OnHyperlinkClicked(string url UNUSED)	// Overriden in derived classes
{

}


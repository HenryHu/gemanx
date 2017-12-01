/**
 * Copyright (c) 2005 PCMan <pcman.tw@gmail.com>
 *                    Chia I Wu <b90201047@ntu.edu.tw>
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
  #pragma implementation "font.h"
#endif


#include "font.h"
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

CFont::CFont()
{
	m_XftFont = NULL;
	m_PointSize = 0;
	m_MaxWidth = 0;
	m_MaxHeight = 0;
	m_AntiAlias = false;
}

CFont::~CFont()
{
	CloseXftFont( m_XftFont );
}

CFont::CFont( string name, int pt_size, bool anti_alias )
{
	m_XftFont = NULL;
	m_Name = name;
   	m_PointSize = pt_size;
	m_AntiAlias = anti_alias;

	m_XftFont = CreateXftFont( name, pt_size, m_AntiAlias );
}

void CFont::SetFont( string name, int pt_size, bool anti_alias )
{
	m_Name = name;
   	m_PointSize = pt_size;
	m_AntiAlias = anti_alias;

	CloseXftFont( m_XftFont );
	m_XftFont = CreateXftFont( name, pt_size, m_AntiAlias );
}

void CFont::CloseXftFont( XftFont* font )
{
	if( font )
	{
		Display *display = gdk_x11_get_default_xdisplay();
		XftFontClose(display, font );
	}
}

XftFont* CFont::CreateXftFont( string name, int size, bool anti_alias )
{
	Display *display = gdk_x11_get_default_xdisplay();
	int screen = DefaultScreen (display);

	XftFont* font = XftFontOpen (display, screen,
					FC_FAMILY, FcTypeString, name.c_str(),
					FC_SIZE, FcTypeDouble, (double)size,
					FC_WEIGHT, FcTypeInteger, FC_WEIGHT_MEDIUM,
					FC_ANTIALIAS, FcTypeBool, anti_alias,
					XFT_CORE, FcTypeBool, False,
					NULL);

	return font;
}

void CFont::SetFontFamily( string name )
{
    SetFont( name, m_PointSize, m_AntiAlias );
}

void CFont::SetFontSize( int pt_size )
{
    SetFont( m_Name, pt_size, m_AntiAlias );
}

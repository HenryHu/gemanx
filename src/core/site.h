/**
 * site.h - interface for the CSite class.
 *                                                                              
 * Copyright (c) 2004-2005 PCMan <pcman.tw@gmail.com>
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

#if !defined(PCMANX_SITE_H)
#define PCMANX_SITE_H

// leads to undefined error...
/*#ifdef __GNUG__
  #pragma interface "site.h"
#endif*/

#include "gemanx_utils.h"

#include <gtk/gtk.h>

#include <stdio.h>
#include <string>

#include "stringutil.h"

class CSite
{
public:
	bool m_Startup;
	X_EXPORT void SaveToFile(FILE* fo);
	// Name of site
    std::string m_Name;

	// IP : port
	std::string m_URL;

	// Time duration in seconds during which should we reconnect
	// automatically when disconnected from server, and 0 means disabled.
	unsigned int m_AutoReconnect;

	// Delay before next reconnect
	unsigned int m_ReconnectInt;

	unsigned int m_ReconnectTimerId;

	// We send this string, m_AntiIdleStr, to the server every 'm_AntiIdle'
	// seconds to prevent being kicked by the server.
	std::string m_AntiIdleStr;	// empty string means '\0'
	unsigned int m_AntiIdle;	// 0 means disabled

	// Site Encoding
	std::string m_Encoding;

	// Detect Double-byte characters?
	bool m_DetectDBChar;
	
	// Terminal settings
	// Rows per page
	unsigned int m_RowsPerPage;
	// Cols per page
	unsigned int m_ColsPerPage;

	// When pasting long articles, especially those from webpages, wrap lines 
	// automatically when there are more than 'm_AutoWrapOnPaste' characters per line.
	unsigned int m_AutoWrapOnPaste;	// 0 means disabled.

	// Convert ESC characters in ANSI color to m_ESCConv
	std::string m_ESCConv;

	// Terminal type
	std::string m_TermType;

	int m_CRLF;
    GtkWidget*  m_MenuItem;
	// Send CR, LF, or CRLF when Enter is pressed
	const char* GetCRLF()
	{
		const char* crlf[3] = { "\r", "\n", "\r\n" };
		return (m_CRLF > 3 ? "\r" : crlf[m_CRLF]);
	}

	std::string GetEscapeChar()	{	return UnEscapeStr(m_ESCConv.c_str());	}

#ifdef USE_EXTERNAL
	bool m_UseExternalSSH;
	bool m_UseExternalTelnet;
#endif
	bool m_bHorizontalCenterAlign;
	bool m_bVerticalCenterAlign;

#ifdef USE_PROXY
	// Proxy settings
	int    m_ProxyType;
	std::string m_ProxyAddr;
	int    m_ProxyPort;
	std::string m_ProxyUser;
	std::string m_ProxyPass;
#endif

	X_EXPORT CSite(std::string Name = "");
	X_EXPORT ~CSite();

    std::string& GetPasswd(){	return m_Passwd;	}
    inline void SetPasswd(const std::string& passwd ){	m_Passwd = passwd;	}

    std::string& GetPasswdPrompt(){	return m_PasswdPrompt;	}
    inline void SetPasswdPrompt(const std::string& passwd_prompt ){	m_PasswdPrompt = passwd_prompt;	}

	std::string& GetLogin(){	return m_Login;	}
    void SetLogin(const std::string& login ){	m_Login = login;	}

	std::string& GetLoginPrompt(){	return m_LoginPrompt;	}
    void SetLoginPrompt(const std::string& login_prompt ){	m_LoginPrompt = login_prompt;	}

    std::string& GetPreLogin(){	return m_PreLogin;	}
    void SetPreLogin(const std::string& prelogin){	m_PreLogin = prelogin;	}

	std::string& GetPreLoginPrompt(){	return m_PreLoginPrompt;	}
    void SetPreLoginPrompt(const std::string& prelogin_prompt ){	m_PreLoginPrompt = prelogin_prompt;	}

	std::string& GetPostLogin(){	return m_PostLogin;	}
    void SetPostLogin(const std::string& postlogin){	m_PostLogin = postlogin;	}

protected:
    std::string m_Passwd;
    std::string m_Login;
    std::string m_LoginPrompt;
    std::string m_PasswdPrompt;
    std::string m_PreLogin;
    std::string m_PreLoginPrompt;
	std::string m_PostLogin;
};

#endif // !defined(PCMANX_SITE_H)

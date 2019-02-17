/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "uva_os_info.hpp"

std::string p_os_to_string(P_OS os)
{
	switch(os)
	{
	case P_OS::All:
		return "ALL";
	case P_OS::Win32:
		return "WIN32";
	case P_OS::Win64:
		return "WIN64";
	case P_OS::Lin32:
		return "LIN32";
	case P_OS::Lin64:
		return "LIN64";
	default:
		return "INVALID";
	};
}

P_OS get_active_system()
{
	if(util::is_linux_system())
	{
		if(util::is_x64_system())
			return P_OS::Lin64;
		return P_OS::Lin32;
	}
	if(!util::is_windows_system())
		return P_OS::Invalid;
	if(util::is_x64_system())
		return P_OS::Win64;
	return P_OS::Win32;
}

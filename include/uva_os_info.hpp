/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UVA_OS_INFO_HPP__
#define __UVA_OS_INFO_HPP__

#include <sharedutils/util.h>
#include <string>
#include <cinttypes>

#undef WIN32
#undef WIN64

enum class P_OS : uint8_t
{
	Invalid = 0,
	All,
	Win32,
	Win64,
	Lin32,
	Lin64
};

std::string p_os_to_string(P_OS os);
P_OS get_active_system();

#endif

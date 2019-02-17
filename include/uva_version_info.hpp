/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UVA_VERSION_INFO_HPP__
#define __UVA_VERSION_INFO_HPP__

#include <vector>
#include <sharedutils/util_version.h>

struct VersionInfo
{
	util::Version version;
	std::vector<uint32_t> files;
};

#endif

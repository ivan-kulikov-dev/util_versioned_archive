/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UVA_DEFINITIONS_HPP__
#define __UVA_DEFINITIONS_HPP__

#ifdef UVA_STATIC
	#define DLLUVA
#elif UVA_DLL
	#ifdef __linux__
		#define DLLUVA __attribute__((visibility("default")))
	#else
		#define DLLUVA __declspec(dllexport)
	#endif
#else
	#ifdef __linux__
		#define DLLUVA
	#else
		#define DLLUVA __declspec(dllimport)
	#endif
#endif

#endif

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UVA_FILEINFO_HPP__
#define __UVA_FILEINFO_HPP__

#include "uva_os_info.hpp"
#include "uva_definitions.hpp"
#include <string>

namespace uva
{
	struct DLLUVA FileInfo
	{
		enum class Flags : uint32_t
		{
			None = 0,
			Directory = 1,
			Windows = Directory<<1,
			Linux = Windows<<1,
			x86 = Linux<<1,
			x64 = x86<<1,
			AllOS = Windows | Linux | x86 | x64
		};
		static Flags os_to_flags(P_OS os);

		FileInfo()=default;
		std::string name;
		int32_t crc = 0;
		Flags flags = Flags::AllOS;
		std::shared_ptr<std::vector<uint8_t>> data = nullptr;
		uint64_t offset = 0;
		uint64_t size = 0;
		uint64_t sizeUncompressed = 0;

		bool IsDirectory() const;
		bool IsFile() const;
		bool IsCompressed() const;
		bool operator==(P_OS os) const;
		bool operator!=(P_OS os) const;
	};
};
REGISTER_BASIC_BITWISE_OPERATORS(uva::FileInfo::Flags);

struct PublishInfo
{
	PublishInfo(const std::string &file,P_OS os,const std::string &src);
	PublishInfo()=default;
	bool operator==(const PublishInfo &other) const;
	bool operator!=(const PublishInfo &other) const;
	bool operator==(const std::string &str) const;
	bool operator!=(const std::string &str) const;
	bool operator==(const P_OS &os) const;
	bool operator!=(const P_OS &os) const;
	bool operator==(const uva::FileInfo &info) const;
	bool operator!=(const uva::FileInfo &info) const;
	std::string file;
	P_OS os = P_OS::All;
	std::string src;
	std::string GetSourceName() const;
};

#endif

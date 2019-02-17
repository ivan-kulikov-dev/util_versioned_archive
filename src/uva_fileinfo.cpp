/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "uva_fileinfo.hpp"
#include <sharedutils/util_file.h>
#include <sharedutils/util_string.h>
#include <fsys/filesystem.h>

uva::FileInfo::Flags uva::FileInfo::os_to_flags(P_OS os)
{
	switch(os)
	{
		case P_OS::Win32:
			return Flags::x86 | Flags::Windows;
			break;
		case P_OS::Win64:
			return Flags::x64 | Flags::Windows;
			break;
		case P_OS::Lin32:
			return Flags::x86 | Flags::Linux;
			break;
		case P_OS::Lin64:
			return Flags::x64 | Flags::Linux;
			break;
		case P_OS::Invalid:
		default:
			break;
	}
	return Flags::None;
}
bool uva::FileInfo::IsDirectory() const {return (flags &Flags::Directory) != Flags::None;}
bool uva::FileInfo::IsFile() const {return !IsDirectory();}
bool uva::FileInfo::IsCompressed() const {return true;}
bool uva::FileInfo::operator==(P_OS os) const
{
	auto osFlags = flags &Flags::AllOS;
	switch(os)
	{
		case P_OS::Win32:
			return osFlags == (Flags::Windows | Flags::x86);
		case P_OS::Win64:
			return osFlags == (Flags::Windows | Flags::x64);
		case P_OS::Lin32:
			return osFlags == (Flags::Linux | Flags::x86);
		case P_OS::Lin64:
			return osFlags == (Flags::Linux | Flags::x64);
		case P_OS::All:
			return osFlags == Flags::AllOS;
		default:
			return true;
	}
}
bool uva::FileInfo::operator!=(P_OS os) const {return !(*this == os);}

PublishInfo::PublishInfo(const std::string &_file,P_OS _os,const std::string &_src)
	: file(_file),os(_os),src(_src)
{}
bool PublishInfo::operator==(const PublishInfo &other) const {return (this->file == other.file && this->os == other.os) ? true : false;}
bool PublishInfo::operator!=(const PublishInfo &other) const {return (*this == other) ? false : true;}
bool PublishInfo::operator==(const std::string &str) const {return (this->file == str) ? true : false;}
bool PublishInfo::operator!=(const std::string &str) const {return (*this == str) ? false : true;}
bool PublishInfo::operator==(const P_OS &os) const {return (this->os == os) ? true : false;}
bool PublishInfo::operator!=(const P_OS &os) const {return (*this == os) ? false : true;}
bool PublishInfo::operator==(const uva::FileInfo &info) const
{
	if(uva::FileInfo::os_to_flags(os) != info.flags)
		return false;
	std::string name = GetSourceName();
	return (name == info.name) ? true : false;
}
bool PublishInfo::operator!=(const uva::FileInfo &info) const {return (*this == info) ? false : true;}
std::string PublishInfo::GetSourceName() const
{
	std::string name = src;
	auto bFileName = (name.find_first_of('.') != ustring::NOT_FOUND) ? true : false;
	if(!name.empty() && bFileName == false)
	{
		if(name.back() == '/')
			name[name.size() -1] = '\\';
		else
			name += '\\';
		if(name.size() == 1 && name.back() == '\\')
			name = "";
	}
	else if(bFileName == true && !name.empty() && name.back() == '.')
		name.erase(name.end() -1);
	if(bFileName == false)
		name += ufile::get_file_from_filename(file);
	return FileManager::GetCanonicalizedPath(name);
}

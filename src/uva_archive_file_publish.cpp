/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "uva_archive_file.hpp"
#include "util_versioned_archive.hpp"
#include <sharedutils/util_file.h>
#include <sharedutils/util_string.h>
#include <iostream>
#pragma warning(disable: 4307)
#pragma warning(disable: 4800)
#pragma warning(disable: 4701)
#include <boost/crc.hpp>
#pragma warning(default: 4701)
#pragma warning(default: 4800)
#pragma warning(default: 4307)

#undef WIN32

extern "C" {
	#include "bzlib.h"
}
#ifdef UVA_VERBOSE
#include <iostream>
#endif

std::string uva::ArchiveFile::result_code_to_string(UpdateResult code)
{
	switch(code)
	{
		case UpdateResult::Success:
			return "Success";
		case UpdateResult::ListFileNotFound:
			return "List file not found";
		case UpdateResult::NothingToUpdate:
			return "Nothing to update";
		case UpdateResult::UnableToCreateArchiveFile:
			return "Unable to create archive file";
		case UpdateResult::VersionDiscrepancy:
			return "Version discrepancy";
		case UpdateResult::UnableToRemoveTemporaryFiles:
			return "Unable to remove temporary files";
		default:
			return "Unknown";
	}
}

uva::ArchiveFile::UpdateResult uva::ArchiveFile::PublishUpdate(
	const std::string &filePath,
	util::Version &version,
	std::vector<PublishInfo> &files,
	const std::string &updateFile,
	const std::function<bool(VFilePtr&)> &readCallback,
	const std::function<bool(VFilePtrReal&)> &writeCallback,
	const std::function<void(std::string&,std::string&,std::vector<uint8_t>&)> &dataTranslateCallback
)
{
	auto f = std::unique_ptr<ArchiveFile>(uva::ArchiveFile::Open(updateFile,readCallback,writeCallback));
	if(f == nullptr)
		return UpdateResult::UnableToCreateArchiveFile;
	auto lastVersion = version;
	if(f->GetLatestVersion(&lastVersion) == true && version <= lastVersion && version != util::Version{})
	{
#ifdef UVA_VERBOSE
		std::cout<<"WARNING: New version ("<<version.ToString()<<") is lower or equal to existing version ("<<lastVersion.ToString()<<")"<<std::endl;
#endif
		return UpdateResult::VersionDiscrepancy;
	}
	if(version == util::Version{})
	{
		version = lastVersion;
		if(version.revision +1 >= 10)
		{
			if(version.minor +1 >= 10)
			{
				++version.major;
				version.minor = 0;
			}
			else
				++version.minor;
			version.revision = 0;
		}
		else
			++version.revision;
	}

	// Normalize Paths
	for(auto &f : files)
	{
		auto &name = f.file;
		name = FileManager::GetCanonicalizedPath(name);
		//std::transform(name.begin(),name.end(),name.begin(),::tolower);
	}

	// Remove duplicates
	for(unsigned int i=static_cast<unsigned int>(files.size()) -1;i>=1;i--)
	{
		for(unsigned int j=i -1;j!=(unsigned int)-1;j--)
		{
			if(files[i] == files[j])
				files.erase(files.begin() +i);
		}
	}
	// Move changelog to top
	auto it = std::find_if(files.begin(),files.end(),[](const PublishInfo &info) {
		return (info.GetSourceName() == "changelog.txt") ? true : false;
	});
	if(it != files.end())
	{
		auto v = *it;
		files.erase(it);
		files.insert(files.begin(),v);
	}
	VersionInfo newVersionInfo;
	newVersionInfo.version = version;

	/*for(auto &info : fileInfo)
	{
		if(info.size > 0)
		{
			auto bExists = false;
			for(auto &file : files)
			{
				auto &file = *it;
				if(file.GetSourceName() == info.name)
				{
					bExists = true;
					break;
				}
			}
			if(bExists == false)
			{
				newVersionInfo.files.push_back(static_cast<unsigned int>(idx));
				info.size = 0;
				std::cout<<"Marked file '"<<info.name<<"' for deletion!"<<std::endl;
			}
		}
	}*/
	//
	uint32_t idx = 0;
	uint32_t numUnchanged = 0;
	uint32_t numAdded = 0;
	uint32_t numChanged = 0;
	uint32_t numDeleted = 0;
	for(auto &file : files)
	{
		auto srcName = file.GetSourceName();

		uint64_t size = 0;
		uint64_t sizeUncompressed = 0;
		std::vector<uint8_t> data;
		auto fptr = FileManager::OpenSystemFile(file.file.c_str(),"rb");
		if(fptr != nullptr)
		{
			sizeUncompressed = fptr->GetSize();
			if(sizeUncompressed == 0)
			{
				size = 0;
//#ifdef UVA_VERBOSE
				std::cout<<"Marked file '"<<file.file<<"' for deletion!"<<std::endl;
				++numDeleted;
//#endif
			}
			else
			{
				data.resize(sizeUncompressed);
				fptr->Read(data.data(),sizeUncompressed);

				if(dataTranslateCallback != nullptr)
				{
					dataTranslateCallback(file.file,srcName,data);
					sizeUncompressed = data.size();
				}
			}
		}

		auto *info = f->FindFile(srcName,idx);
		auto bExists = (info != nullptr) ? true : false;
		if(info != nullptr)
		{
			newVersionInfo.files.push_back(idx);
#ifdef UVA_VERBOSE
			std::cout<<"Adding existing file "<<idx<<" to update"<<std::endl;
#endif
		}
		if(info == nullptr)
		{
			info = f->AddFile(srcName,idx);
			newVersionInfo.files.push_back(idx);
//#ifdef UVA_VERBOSE
			++numAdded;
			std::cout<<"Adding new file '"<<srcName<<"' to update"<<std::endl;
//#endif
		}
		/*FileInfo *info = nullptr;
		bool bExists = false;
		for(auto j=0;j<fileInfo.size();j++)
		{
			FileInfo &fi = fileInfo[j];
			if(file == fi)
			{
				info = &fi;
				newVersionInfo.files.push_back(j);
				bExists = true;
				break;
			}
		}*/
		/*if(info == nullptr)
		{
			FileInfo fi;
			fileInfo.push_back(fi);
			newVersionInfo.files.push_back(static_cast<unsigned int>(fileInfo.size() -1));
			info = &fileInfo[fileInfo.size() -1];
		}*/
		//info->name = file.GetSourceName();
		info->flags |= FileInfo::os_to_flags(file.os);

		if(fptr != nullptr)
		{
			info->sizeUncompressed = sizeUncompressed;
			if(info->sizeUncompressed == 0)
			{
				info->size = 0;
//#ifdef UVA_VERBOSE
				std::cout<<"Marked file '"<<file.file<<"' for deletion!"<<std::endl;
//#endif
				++numDeleted;
			}
			else
			{
				//std::cout<<"DATA: "<<data<<std::endl;
				boost::crc_32_type crc;
				crc.process_bytes(data.data(),info->sizeUncompressed);
				bool bSkip = false;
				if(bExists) // Compare with existing version
				{
					if(info->crc == crc.checksum())
					{
						bSkip = true;
						numUnchanged++;
#ifdef UVA_VERBOSE
						std::cout<<"WARNING: File '"<<info->name<<"' hasn't changed! Skipping..."<<std::endl;
#endif
						newVersionInfo.files.pop_back();
					}
//#ifdef UVA_VERBOSE
					else
					{
						++numChanged;
						std::cout<<"'"<<info->name<<"' has been changed."<<std::endl;
					}
//#endif
				}
#ifdef UVA_VERBOSE
				else
					std::cout<<"Adding '"<<info->name<<"'..."<<std::endl;
#endif
				if(!bSkip)
				{
					info->crc = crc.checksum();
					int32_t blockSize100k = 8;
					int32_t verbosity = 0;
					int32_t workFactor = 30;
					auto destLength = static_cast<uint32_t>(1.01 *static_cast<double>(info->sizeUncompressed) +600.0);
					auto dest = std::make_shared<std::vector<uint8_t>>(destLength);
#ifdef UVA_VERBOSE
					std::cout<<"Compressing...";
#endif
					auto err = BZ2_bzBuffToBuffCompress(reinterpret_cast<char*>(dest->data()),&destLength,reinterpret_cast<char*>(data.data()),static_cast<unsigned int>(info->sizeUncompressed),blockSize100k,verbosity,workFactor);
					if(err == BZ_OK)
					{
#ifdef UVA_VERBOSE
						std::cout<<" Done!"<<std::endl;
#endif
						info->data = std::move(dest);
						info->size = destLength;
					}
					else
					{
//#ifdef UVA_VERBOSE
						std::cout<<std::endl<<"WANRING: Unable to compress file '"<<file.file<<"'! ("<<err<<")"<<std::endl;
//#endif
						info->size = 0;
					}
				}
			}
		}
		else
		{
			info->size = 0;
			info->sizeUncompressed = 0;
//#ifdef UVA_VERBOSE
			std::cout<<"Marked file '"<<file.file<<"' for deletion!"<<std::endl;
			++numDeleted;
//#endif
		}
	}

	// Check for removed files, has to be done after data translation!
	auto &root = f->GetRoot();
	std::function<void(uva::ArchiveFile::FileIndexInfo&,std::string)> fIterateHierarchy = nullptr;
	fIterateHierarchy = [&fIterateHierarchy,&f,&files,&newVersionInfo,&numDeleted](uva::ArchiveFile::FileIndexInfo &fii,std::string path) {
		if(path.empty() == false)
			path += "\\";
		auto fi = f->GetByIndex(fii.index);
		path += fi->name;
		if(fi->IsDirectory() == false && fi->size > 0)
		{
			auto it = std::find_if(files.begin(),files.end(),[&path](const PublishInfo &pi) {
				return ustring::compare(path,pi.GetSourceName(),false);
			});
			if(it == files.end())
			{
				newVersionInfo.files.push_back(static_cast<unsigned int>(fii.index));
				fi->size = 0;
				++numDeleted;
#ifdef UVA_VERBOSE
				std::cout<<"Marked file '"<<fi->name<<"' for deletion!"<<std::endl;
#endif
			}
		}
		for(auto &child : fii.children)
			fIterateHierarchy(*child,path);
	};
	fIterateHierarchy(root,"");

	if(numAdded == 0 && numChanged == 0 && numDeleted == 0)
	{
#ifdef UVA_VERBOSE
		std::cout<<"WARNING: Nothing to update!"<<std::endl;
#endif
		return UpdateResult::NothingToUpdate;
	}
//#ifdef UVA_VERBOSE
	std::cout<<numUnchanged<<" files are unchanged and have been skipped!"<<std::endl;
	std::cout<<numAdded<<" files have been added!"<<std::endl;
	std::cout<<numChanged<<" files have been changed!"<<std::endl;
	std::cout<<numDeleted<<" files have been deleted!"<<std::endl;
//#endif
	//for(unsigned int i=0;i<newVersionInfo.files.size();i++)
	//	std::cout<<"FILE: "<<newVersionInfo.files[i]<<std::endl;
	/*if(newVersionInfo.files.empty())
	{
#ifdef UVA_VERBOSE
		std::cout<<"WARNING: Nothing to update!"<<std::endl;
#endif
		return UpdateResult::NothingToUpdate;
	}*/
	f->AddVersion(newVersionInfo);
	if(f->Export() == false)
	{
#ifdef UVA_VERBOSE
		std::cout<<"WARNING: Unable to rename versioninfo_tmp.dat"<<std::endl;
#endif
		return UpdateResult::UnableToRemoveTemporaryFiles;
	}
#ifdef UVA_VERBOSE
	std::cout<<"Publishing was successful! "<<newVersionInfo.files.size()<<" files have been updated."<<std::endl;
#endif
	return UpdateResult::Success;
}

static void find_all_files(std::string path,const std::function<void(std::string,std::string)> &f)
{
	path = FileManager::GetCanonicalizedPath(path);
	std::vector<std::string> files;
	std::vector<std::string> dirs;
	FileManager::FindSystemFiles((path +"/*").c_str(),&files,&dirs);
	for(auto it=files.begin();it!=files.end();++it)
		f(path +std::string("/"),*it);
	for(auto it=dirs.begin();it!=dirs.end();++it)
		find_all_files(path +std::string("/") +(*it),f);
}

uva::ArchiveFile::UpdateResult uva::ArchiveFile::PublishUpdate(
	util::Version &version,
	const std::string &updateListFile,
	const std::string &archiveFile,
	const std::function<bool(VFilePtr&)> &readCallback,
	const std::function<bool(VFilePtrReal&)> &writeCallback,
	const std::function<void(std::string&,std::string&,std::vector<uint8_t>&)> &dataTranslateCallback
)
{
	auto flist = updateListFile;
	std::string ext;
	if(ufile::get_extension(flist,&ext) == false)
		flist += ".txt";
	std::string pathToFiles;
	auto f = FileManager::OpenFile<VFilePtrReal>(flist.c_str(),"r");
	if(f == nullptr)
	{
		f = FileManager::OpenSystemFile(flist.c_str(),"r");
		if(f == nullptr)
			return UpdateResult::ListFileNotFound;
		pathToFiles = ufile::get_path_from_filename(flist);
	}
	else
		pathToFiles = FileManager::GetProgramPath() +ufile::get_path_from_filename(flist);
	auto c = FileManager::GetDirectorySeparator();
	if(pathToFiles.empty() == false && pathToFiles.back() != c)
		pathToFiles += c;
	std::vector<PublishInfo> fileInfo;
	while(!f->Eof())
	{
		std::string l = f->ReadLine();
		std::vector<std::string> argv = ustring::get_args(l);
		if(!argv.empty())
		{
			auto f = pathToFiles +argv.front();
			f = FileManager::GetCanonicalizedPath(f);
			std::unordered_map<std::string,std::string> keyvalues;
			for(auto i=1;i<argv.size();i++)
			{
				std::string &str = argv[i];
				std::vector<std::string> strSub;
				ustring::explode(str,"=",strSub);
				if(strSub.size() > 1)
				{
					ustring::remove_whitespace(strSub[0]);
					ustring::remove_whitespace(strSub[1]);
					if(!strSub[0].empty() && !strSub[1].empty())
						keyvalues.insert(std::unordered_map<std::string,std::string>::value_type(strSub[0],strSub[1]));
				}
			}
			P_OS os = P_OS::All;
			auto itOS = keyvalues.find("os");
			if(itOS != keyvalues.end())
			{
				auto &strOS = itOS->second;
				if(strOS == "win32")
					os = P_OS::Win32;
				else if(strOS == "win64")
					os = P_OS::Win64;
				else if(strOS == "lin32")
					os = P_OS::Lin32;
				else if(strOS == "lin64")
					os = P_OS::Lin64;
			}
			std::string src;
			auto itSrc = keyvalues.find("src");
			if(itSrc != keyvalues.end())
				src = itSrc->second;
			std::string sub;
			if(f.length() > 3 && ((sub = f.substr(f.length() -3)) == "/**" || sub == "\\**") && FileManager::IsSystemDir(f.substr(0,f.length() -3)) == true)
			{
				f = f.substr(0,f.length() -3);
				auto fLen = f.length();
				find_all_files(f,[&fileInfo,&os,&src,&fLen](std::string path,std::string file) {
					auto localPath = path.substr(fLen +1,path.length());
					if(!localPath.empty())
					{
						localPath = "/" +localPath;
						if(localPath.back() == '/' || localPath.back() == '\\')
							localPath = localPath.substr(0,localPath.length() -1);
					}
					fileInfo.push_back(PublishInfo(FileManager::GetCanonicalizedPath(path +file),os,FileManager::GetCanonicalizedPath(src +localPath)));
				});
			}
			else
			{
				std::vector<std::string> files;
				FileManager::FindSystemFiles(f.c_str(),&files,nullptr,true);
				for(auto it=files.begin();it!=files.end();it++)
					fileInfo.push_back(PublishInfo(ufile::get_path_from_filename(f) +*it,os,src));
			}
		}
	}
	if(fileInfo.empty())
		return UpdateResult::NothingToUpdate;
	return PublishUpdate(pathToFiles,version,fileInfo,archiveFile,readCallback,writeCallback,dataTranslateCallback);
}

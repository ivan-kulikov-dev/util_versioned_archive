/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "uva_archive_file.hpp"
#include <array>
#include <sharedutils/util_string.h>
#include <iostream>
#include <cstring>

extern "C" {
	#include "bzlib.h"
}

const std::array<char,5> ARCHIVE_IDENT = {'V','A','R','C','H'};
const uint32_t ARCHIVE_VERSION = 1;

bool uva::ArchiveFile::ReadHeader()
{
	auto &f = m_in;
	std::array<char,ARCHIVE_IDENT.size()> ident;
	f->Read(ident.data(),ident.size());
	if(strncmp(ident.data(),ARCHIVE_IDENT.data(),ident.size()) != 0)
		return false;
	auto version = f->Read<uint32_t>();

	auto hdVersionOffset = f->Read<uint64_t>();
	auto hdFileOffset = f->Read<uint64_t>();
	auto hdFileNameOffset = f->Read<uint64_t>();
	auto hdHierarchyOffset = f->Read<uint64_t>();
	auto hdDataOffset = f->Read<uint64_t>();

	return true;
}

void uva::ArchiveFile::ReadVersionLayer()
{
	auto &f = m_in;
	if(f == nullptr)
		return;
	auto numVersions = f->Read<uint32_t>();
	for(auto i=decltype(numVersions){0};i<numVersions;++i)
	{
		VersionInfo info;
		info.version = f->Read<util::Version>();
		auto numFiles = f->Read<uint32_t>();
		info.files.reserve(numFiles);
		for(auto j=decltype(numFiles){0};j<numFiles;++j)
		{
			auto idxFile = f->Read<uint32_t>();
			info.files.push_back(idxFile);
		}
		m_versions.push_back(info);
	}
}

void uva::ArchiveFile::ReadFiles()
{
	auto &f = m_in;
	if(f == nullptr)
		return;
	auto numFiles = f->Read<uint32_t>();
	m_files.reserve(numFiles);
	for(auto i=decltype(numFiles){0};i<numFiles;++i)
	{
		m_files.push_back(std::make_shared<uva::FileInfo>());
		auto &fi = m_files.back();
		auto fh = f->Read<FileHeader>();
		fi->flags = static_cast<uva::FileInfo::Flags>(fh.flags);
		fi->size = fh.size;
		fi->sizeUncompressed = fh.sizeUncompressed;
		fi->offset = fh.offset;
		fi->crc = fh.crc;
	}
}

void uva::ArchiveFile::ReadFileNames()
{
	auto &f = m_in;
	if(f == nullptr)
		return;
	for(auto &fi : m_files)
		fi->name = f->ReadString();
}

void uva::ArchiveFile::ReadFileHierarchy()
{
	auto &f = m_in;
	if(f == nullptr)
		return;
	std::vector<std::shared_ptr<FileIndexInfo>> fileIndexInfo(m_files.size(),nullptr);
	for(auto i=decltype(m_files.size()){0};i<m_files.size();++i)
	{
		auto parentId = f->Read<uint32_t>();
		std::shared_ptr<FileIndexInfo> fiiParent = nullptr;
		if(parentId == std::numeric_limits<decltype(parentId)>::max())
			fiiParent = m_root;
		else
		{
			auto &fii = fileIndexInfo.at(parentId);
			if(fii == nullptr)
			{
				fii = std::make_shared<FileIndexInfo>();
				fii->index = parentId;
			}
			fiiParent = fii;
		}
		auto &fiiChild = fileIndexInfo.at(i);
		if(fiiChild == nullptr)
		{
			if(i == 0)
				fiiChild = m_root;
			else
			{
				fiiChild = std::make_shared<FileIndexInfo>();
				fiiChild->index = i;
			}
		}
		if(i != 0) // Don't add root as child of itself
		{
			fiiChild->parent = fiiParent;
			fiiParent->children.push_back(fiiChild);
		}
	}
	/*std::function<void(FileIndexInfo&)> fReadHierarchy = nullptr;
	fReadHierarchy = [&fReadHierarchy,&f](FileIndexInfo &fii) {
		fii.index = f->Read<uint32_t>();
		auto numChildren = f->Read<uint32_t>();
		for(auto i=decltype(numChildren){0};i<numChildren;++i)
		{
			fii.children.push_back(std::make_shared<FileIndexInfo>());
			fii.children.back()->parent = fii.shared_from_this();
			fReadHierarchy(*fii.children.back());
		}
	};
	fReadHierarchy(*m_root);*/
	// New concept:
	// Use table
	// [fileId] = parentId
	//!!!
}

void uva::ArchiveFile::ReadFileData(uint64_t startOffset,const uva::FileInfo &fi,std::vector<uint8_t> &data) const
{
	auto &f = m_in;
	if(f == nullptr)
		return;
	auto offset = f->Tell();
	f->Seek(startOffset +fi.offset);
	data.resize(fi.size);
	f->Read(data.data(),data.size());
	f->Seek(offset);
}

static void write_offset(const VFilePtrReal &f,uint64_t startOffset,uint64_t offsetToOffsetLocation)
{
	auto offset = f->Tell();
	f->Seek(offsetToOffsetLocation);
	f->Write<uint64_t>(offset -startOffset);
	f->Seek(offset);
}

void uva::ArchiveFile::WriteHeader(uint64_t &hdVersionOffset,uint64_t &hdFileOffset,uint64_t &hdFileNameOffset,uint64_t &hdHierarchyOffset,uint64_t &hdDataOffset)
{
	auto &f = m_out;
	f->Write(ARCHIVE_IDENT.data(),ARCHIVE_IDENT.size());
	f->Write<uint32_t>(ARCHIVE_VERSION);

	hdVersionOffset = f->Tell();
	f->Write<uint64_t>(0u); // Version layer ofset
	hdFileOffset = f->Tell();
	f->Write<uint64_t>(0u); // File layer offset
	hdFileNameOffset = f->Tell();
	f->Write<uint64_t>(0u); // File name layer offset
	hdHierarchyOffset = f->Tell();
	f->Write<uint64_t>(0u); // File hierarchy offset
	hdDataOffset = f->Tell();
	f->Write<uint64_t>(0u); // File data offset
}

void uva::ArchiveFile::WriteVersionLayer()
{
	auto &f = m_out;
	f->Write<uint32_t>(static_cast<uint32_t>(m_versions.size()));
	for(auto &info : m_versions)
	{
		f->Write<util::Version>(info.version);
		f->Write<uint32_t>(static_cast<uint32_t>(info.files.size()));
		for(auto id : info.files)
			f->Write<uint32_t>(id);
	}
}

void uva::ArchiveFile::WriteFiles(uint64_t &fileHeaderOffset)
{
	auto &f = m_out;
	f->Write<uint32_t>(m_files.size());
	fileHeaderOffset = f->Tell();
	for(auto &fi : m_files)
	{
		FileHeader fh {};
		fh.flags = umath::to_integral(fi->flags);
		fh.size = fi->size;
		fh.sizeUncompressed = fi->sizeUncompressed;
		fh.crc = fi->crc;
		f->Write<FileHeader>(fh);
	}
}

void uva::ArchiveFile::WriteFileNames(uint64_t startOffset,uint64_t fileHeaderOffset)
{
	auto &f = m_out;
	for(auto i=decltype(m_files.size()){0};i<m_files.size();++i)
	{
		write_offset(f,startOffset,fileHeaderOffset +sizeof(FileHeader) *i +offsetof(FileHeader,fileNameOffset));
		auto &fi = m_files.at(i);
		f->WriteString(fi->name);
	}
}

void uva::ArchiveFile::WriteFileHierarchy()
{
	auto &f = m_out;
	std::vector<uint32_t> parentIds(m_files.size(),std::numeric_limits<uint32_t>::max());
	std::function<void(FileIndexInfo&)> fIterateHierarchy = nullptr;
	fIterateHierarchy = [&fIterateHierarchy,&parentIds](FileIndexInfo &fii) {
		auto parentId = std::numeric_limits<uint32_t>::max();
		if(fii.parent.expired() == false)
			parentId = fii.parent.lock()->index;
		parentIds.at(fii.index) = parentId;

		for(auto &child : fii.children)
			fIterateHierarchy(*child);
	};
	fIterateHierarchy(*m_root);

	for(auto idx : parentIds)
		f->Write<uint32_t>(idx);
	/*std::function<void(FileIndexInfo&)> fWriteHierarchy = nullptr;
	fWriteHierarchy = [&fWriteHierarchy,&f](FileIndexInfo &fii) {
		f->Write<uint32_t>(fii.index);
		f->Write<uint32_t>(fii.children.size());
		for(auto &child : fii.children)
			fWriteHierarchy(*child);
	};
	fWriteHierarchy(*m_root);*/
	/*f->Write<uint32_t>(static_cast<uint32_t>(m_fileInfo.size()));
	for(auto &info : m_fileInfo)
	{
		f->WriteString(info.name);
		f->Write<uint8_t>(static_cast<uint8_t>(info.os));
		f->Write<uint64_t>(info.size);
		if(info.size > 0)
		{
			f->Write<uint64_t>(info.sizeUncompressed);
			info.offsetToStringTable = f->Tell();
			f->Write<uint64_t>(info.offset);
			f->Write<uint32_t>(info.crc);
		}
	}*/
}

void uva::ArchiveFile::WriteFileData(uint64_t startOffset,uint64_t fileHeaderOffset)
{
	for(auto i=decltype(m_files.size()){0};i<m_files.size();++i)
	{
		auto &fi = m_files.at(i);
		if(fi->size == 0)
			continue;
		write_offset(m_out,startOffset,fileHeaderOffset +sizeof(FileHeader) *i +offsetof(FileHeader,offset));
		if(fi->data != nullptr)
			m_out->Write(fi->data->data(),fi->size);
		else if(m_in != nullptr)
		{
			m_in->Seek(m_inFileStartOffset +fi->offset); // Old offset
			std::vector<uint8_t> data(fi->size);
			m_in->Read(data.data(),fi->size);
			m_out->Write(data.data(),fi->size);
		}
	}
}

uva::ArchiveFile *uva::ArchiveFile::Open(
	const std::string &updateFileName,
	const std::function<bool(VFilePtr&)> &readCallback,
	const std::function<bool(VFilePtrReal&)> &writeCallback
)
{
	auto in = FileManager::OpenFile<VFilePtrReal>(updateFileName.c_str(),"rb");
	if(in == nullptr)
		in = FileManager::OpenSystemFile(updateFileName.c_str(),"rb");
	return new ArchiveFile(updateFileName,in,readCallback,writeCallback);
}

uva::ArchiveFile::ArchiveFile(
	const std::string &updateFileName,VFilePtrReal &f,
	const std::function<bool(VFilePtr&)> &readCallback,
	const std::function<bool(VFilePtrReal&)> &writeCallback
)
	: m_in(f),m_out(nullptr),m_updateFile(updateFileName),
	m_fReadCallback(readCallback),m_fWriteCallback(writeCallback)
{
	m_root = std::make_shared<FileIndexInfo>();

	if(m_in == nullptr)
	{
		m_files.push_back(std::make_shared<uva::FileInfo>());
		auto &fi = m_files.back();
		fi->flags |= uva::FileInfo::Flags::Directory;
		return;
	}
	if(m_fReadCallback != nullptr && m_fReadCallback(m_in) == false)
		return;
	auto startOffset = m_inFileStartOffset = m_in->Tell();
	if(ReadHeader() == false)
		return;
	ReadVersionLayer();
	ReadFiles();
	ReadFileNames();
	ReadFileHierarchy();
}

uva::ArchiveFile::~ArchiveFile()
{
	Close();
	if(m_in != nullptr)
		m_in = nullptr;
	FileManager::RemoveFile((m_updateFile +std::string("_tmp.dat")).c_str());
}

VFilePtr &uva::ArchiveFile::GetFile() {return m_in;}

void uva::ArchiveFile::GetUpdateFiles(const util::Version &version,std::vector<uint32_t> &updateFiles) const
{
	for(auto &versionOther : m_versions)
	{
		if(version >= versionOther.version)
			break;
		updateFiles.reserve(updateFiles.size() +versionOther.files.size());
		for(auto idx : versionOther.files)
			updateFiles.push_back(idx);
	}
}
bool uva::ArchiveFile::Export()
{
	auto updateFileName = m_updateFile;
	auto tmpName = updateFileName +std::string("_tmp.dat");
	auto f = FileManager::OpenFile<VFilePtrReal>(tmpName.c_str(),"wb");
	if(f == nullptr)
		f = FileManager::OpenSystemFile(tmpName.c_str(),"wb");
	else
		updateFileName = FileManager::GetProgramPath() +FileManager::GetDirectorySeparator() +updateFileName;
	if(f == nullptr)
		return false;
	m_out = f;
	if(m_fWriteCallback != nullptr && m_fWriteCallback(f) == false)
		return false;
	uint64_t hdVersionOffset = 0;
	uint64_t hdFileOffset = 0;
	uint64_t hdFileNameOffset = 0;
	uint64_t hdHierarchyOffset = 0;
	uint64_t hdDataOffset = 0;
	auto startOffset = f->Tell();
	WriteHeader(hdVersionOffset,hdFileOffset,hdFileNameOffset,hdHierarchyOffset,hdDataOffset);

	write_offset(f,startOffset,hdVersionOffset);
	WriteVersionLayer();

	write_offset(f,startOffset,hdFileOffset);
	uint64_t fileHeaderOffset = 0;
	WriteFiles(fileHeaderOffset);

	write_offset(f,startOffset,hdFileNameOffset);
	WriteFileNames(startOffset,fileHeaderOffset);

	write_offset(f,startOffset,hdHierarchyOffset);
	WriteFileHierarchy();

	write_offset(f,startOffset,hdDataOffset);
	WriteFileData(startOffset,fileHeaderOffset);
	/*auto offset = m_out->Tell();
	auto old = offset;
	for(auto &info : m_fileInfo)
	{
		if(info.size != 0)
		{
			m_out->Seek(info.offsetToStringTable);
			m_out->Write<uint64_t>(offset);
			offset += info.size;
		}
	}
	m_out->Seek(old);*/
	Close();
	f = nullptr;

	auto r = true;
	if(FileManager::ExistsSystem((updateFileName +std::string("_bak.dat")).c_str()))
		r = FileManager::RemoveSystemFile((updateFileName +std::string("_bak.dat")).c_str());
	if(r == true && FileManager::ExistsSystem(updateFileName.c_str()))
		r = FileManager::RenameSystemFile(updateFileName.c_str(),(updateFileName +std::string("_bak.dat")).c_str());
	if(r == true)
		r = FileManager::RenameSystemFile((updateFileName +std::string("_tmp.dat")).c_str(),updateFileName.c_str());
	m_in = FileManager::OpenSystemFile(updateFileName.c_str(),"rb");
	return r;
}
uva::FileInfo *uva::ArchiveFile::AddFile(const std::string &fname)
{
	uint32_t idx = 0;
	return AddFile(fname,idx);
}

uva::FileInfo *uva::ArchiveFile::AddFile(const std::string &fname,uint32_t &idx)
{
	auto *fi = FindFile(fname,idx);
	if(fi != nullptr)
		return fi;
	idx = 0;
	auto bIsDir = FileManager::IsDir(fname);
	auto npath = FileManager::GetCanonicalizedPath(fname);
	std::vector<std::string> subPaths;
	ustring::explode(npath,std::string(1,FileManager::GetDirectorySeparator()).c_str(),subPaths);
	if(subPaths.empty() == true)
		return nullptr;

	std::function<FileIndexInfo*(FileIndexInfo&,uint32_t)> fAddFile = nullptr;
	fAddFile = [this,&fAddFile,&subPaths](FileIndexInfo &fii,uint32_t subPathIdx) -> FileIndexInfo* {
		auto &subPath = subPaths.at(subPathIdx);
		auto it = std::find_if(fii.children.begin(),fii.children.end(),[this,&subPath](const std::shared_ptr<FileIndexInfo> &fii) {
			return ustring::compare(subPath,m_files.at(fii->index)->name,false);
		});
		if(it == fii.children.end())
		{
			m_files.push_back(std::make_shared<uva::FileInfo>());
			auto &fi = m_files.back();
			fi->name = subPath;
			if(subPathIdx < subPaths.size() -1)
				fi->flags |= uva::FileInfo::Flags::Directory;

			fii.children.push_back(std::make_shared<FileIndexInfo>());
			fii.children.back()->parent = fii.shared_from_this();
			it = fii.children.end() -1;
			(*it)->index = m_files.size() -1;
		}
		return (subPathIdx < subPaths.size() -1) ? fAddFile(*(*it),subPathIdx +1) : it->get();
	};
	auto *fii = fAddFile(*m_root,0);
	idx = fii->index;
	return m_files.at(fii->index).get();
	/*std::function<uva::FileInfo*(uva::FileInfo&,std::vector<std::string>&)> fCreatePath = nullptr;
	fCreatePath = [this,&fCreatePath,&idx,bIsDir](uva::FileInfo &fi,std::vector<std::string> &subPaths) -> uva::FileInfo* {
		if(subPaths.empty() == true)
		{
			if(bIsDir == false)
				fi.flags &= ~uva::FileInfo::Flags::Directory;
			return &fi;
		}
		auto &path = subPaths.front();
		auto it = std::find_if(fi.children.begin(),fi.children.end(),[&path](const std::shared_ptr<uva::FileInfo> &fiOther) {
			return ustring::compare(path,fiOther->name,false);
		});
		if(it == fi.children.end())
		{
			fi.children.push_back(std::make_shared<uva::FileInfo>());
			it = fi.children.end() -1;
			(*it)->parent = fi.shared_from_this();
			(*it)->name = path;
			(*it)->flags |= uva::FileInfo::Flags::Directory;
			(*it)->index = idx;
			if(idx >= m_indexedFiles.size())
				m_indexedFiles.resize(idx +1);
			m_indexedFiles.at(idx) = *it;
		}
		subPaths.erase(subPaths.begin());
		idx += it -fi.children.begin();
		m_nextIndex = idx;
		return fCreatePath(*(*it),subPaths);
	};
	return fCreatePath(*m_root,subPaths);*/
}

bool uva::ArchiveFile::ExtractAndDecompress(const uva::FileInfo &fi,std::vector<uint8_t> &data) const
{
	std::vector<uint8_t> compressedData;
	ReadFileData(m_inFileStartOffset,fi,compressedData);
	if(compressedData.empty() == false)
	{
		// Decompress data
		int32_t verbosity = 0;
		int32_t small = 0;
		auto szUncompressed = static_cast<uint32_t>(fi.sizeUncompressed);
		data.resize(szUncompressed);

		auto err = BZ2_bzBuffToBuffDecompress(
			reinterpret_cast<char*>(data.data()),
			&szUncompressed,
			reinterpret_cast<char*>(compressedData.data()),
			compressedData.size(),
			small,
			verbosity
		);
		if(err == BZ_OK)
			return true;
	}
	return false;
}

bool uva::ArchiveFile::Extract(const uva::FileInfo &fi,const std::string &outName) const
{
	std::vector<uint8_t> uncompressedData;
	if(ExtractAndDecompress(fi,uncompressedData) == false)
		return false;
	auto f = FileManager::OpenSystemFile(outName.c_str(),"wb");
	if(f != nullptr)
		f->Write(uncompressedData.data(),uncompressedData.size());
	return true;
}

bool uva::ArchiveFile::ExtractData(const std::string &fname,std::vector<uint8_t> &data) const
{
	uint32_t idx = 0;
	auto *fi = FindFile(fname,idx);
	if(fi == nullptr || fi->IsFile() == false)
		return false;
	if(ExtractAndDecompress(*fi,data) == false)
		return false;
	return true;
}

bool uva::ArchiveFile::ExtractFile(const std::string &fname,const std::string &outName) const
{
	uint32_t idx = 0;
	auto *fi = FindFile(fname,idx);
	if(fi == nullptr || fi->IsFile() == false)
		return false;
	return Extract(*fi,outName);
}

bool uva::ArchiveFile::ExtractFile(const std::string &fname) const
{
	uint32_t idx = 0;
	auto *fi = FindFile(fname,idx);
	if(fi == nullptr || fi->IsFile() == false)
		return false;
	return Extract(*fi,fname);
}

void uva::ArchiveFile::ExtractAll(const std::string &path) const
{
	if(m_files.empty() == true)
		return;
	auto npath = FileManager::GetCanonicalizedPath(path);
	auto c = FileManager::GetDirectorySeparator();
	if(npath.empty() == true || npath.back() != c)
		npath += c;
	std::function<void(uva::ArchiveFile::FileIndexInfo&,const std::string&)> fExtractFiles = nullptr;
	fExtractFiles = [this,&npath,&fExtractFiles](uva::ArchiveFile::FileIndexInfo &fii,const std::string &path) {
		auto &fi = m_files.at(fii.index);
		auto fpath = path +fi->name;
		if(fi->IsFile())
		{
			if(Extract(*fi,fpath) == false)
				std::cout<<"WARNING: Unable to extract file '"<<fpath<<"'!"<<std::endl;
		}
		else
		{
			auto lpath = ustring::sub(fpath,path.length());
			FileManager::CreateSystemPath(path,lpath.c_str());
		}
		if(fii.children.empty() == false)
		{
			if(fpath.empty() == false)
				fpath += '\\';
			for(auto &child : fii.children)
				fExtractFiles(*child,fpath);
		}
	};
	if(npath.length() > 0 && npath.back() == c)
		npath.pop_back();
	fExtractFiles(*m_root,npath);
}

uva::FileInfo *uva::ArchiveFile::FindFile(const std::string &fname) const
{
	uint32_t idx;
	return FindFile(fname,idx);
}

uva::FileInfo *uva::ArchiveFile::FindFile(const std::string &fname,uint32_t &idx) const
{
	//if(*fi != P_OS::All && *fi != os)
	//	return nullptr;
	auto nname = FileManager::GetCanonicalizedPath(fname);
	std::vector<std::string> subPaths;
	ustring::explode(nname,std::string(1,FileManager::GetDirectorySeparator()).c_str(),subPaths);

	std::function<FileIndexInfo*(FileIndexInfo&,uint32_t)> fFindFile = nullptr;
	fFindFile = [this,&fFindFile,&subPaths](FileIndexInfo &fii,uint32_t subPathIdx) -> FileIndexInfo* {
		if(subPathIdx >= subPaths.size())
			return nullptr;
		auto &subPath = subPaths.at(subPathIdx);
		auto it = std::find_if(fii.children.begin(),fii.children.end(),[this,&subPath](const std::shared_ptr<FileIndexInfo> &fii) {
			auto &fi = m_files.at(fii->index);
			return ustring::compare(subPath,fi->name,false);
		});
		if(it == fii.children.end())
			return nullptr;
		return (subPathIdx == subPaths.size() -1) ? it->get() : fFindFile(*(*it),subPathIdx +1);
	};
	auto *fii = fFindFile(*m_root,0);
	idx = 0;
	if(fii == nullptr)
		return nullptr;
	idx = fii->index;
	return m_files.at(fii->index).get();
}

void uva::ArchiveFile::SearchFiles(const std::string &searchPattern,std::vector<uva::FileInfo*> &results) const
{
	auto nname = FileManager::GetCanonicalizedPath(searchPattern);
	std::vector<std::string> subPaths;
	ustring::explode(nname,std::string(1,FileManager::GetDirectorySeparator()).c_str(),subPaths);

	std::function<void(FileIndexInfo&,uint32_t)> fFindFiles = nullptr;
	fFindFiles = [this,&fFindFiles,&subPaths,&results](FileIndexInfo &fii,uint32_t subPathIdx) {
		if(subPathIdx >= subPaths.size())
			return;
		auto &subPath = subPaths.at(subPathIdx);
		auto bLastIteration = (subPathIdx == subPaths.size() -1) ? true : false;
		for(auto &child : fii.children)
		{
			auto &fi = m_files.at(child->index);
			if(ustring::match(fi->name,subPath) == true)
			{
				if(bLastIteration == true)
					results.push_back(fi.get());
				else
					fFindFiles(*child,subPathIdx +1);
			}
		}
	};
	fFindFiles(*m_root,0);
}

const std::vector<std::shared_ptr<uva::FileInfo>> &uva::ArchiveFile::GetFiles() const {return m_files;}
std::shared_ptr<uva::FileInfo> uva::ArchiveFile::GetByIndex(uint32_t idx)
{
	if(idx >= m_files.size())
		return nullptr;
	return m_files.at(idx);
}
uva::ArchiveFile::FileIndexInfo *uva::ArchiveFile::FindFileIndexInfo(FileInfo &fi) const
{
	auto it = std::find_if(m_files.begin(),m_files.end(),[&fi](const std::shared_ptr<FileInfo> &fiOther) {
		return (&fi == fiOther.get()) ? true : false;
	});
	if(it == m_files.end())
		return nullptr;
	auto idx = it -m_files.begin();
	std::function<FileIndexInfo*(FileIndexInfo&)> fFindIndexInfo = nullptr;
	fFindIndexInfo = [&fFindIndexInfo,idx](FileIndexInfo &fii) -> FileIndexInfo* {
		if(fii.index == idx)
			return &fii;
		for(auto &child : fii.children)
		{
			auto *fii = fFindIndexInfo(*child);
			if(fii != nullptr)
				return fii;
		}
		return nullptr;
	};
	return fFindIndexInfo(*m_root);
}
std::string uva::ArchiveFile::GetFullPath(FileIndexInfo *fii) const
{
	auto *fi = m_files.at(fii->index).get();
	auto path = fi->name;
	while(fii->parent.expired() == false)
	{
		auto parent = fii->parent.lock();
		path = m_files.at(parent->index)->name +'\\' +path;
		fii = parent.get();
	}
	return path;
}

void uva::ArchiveFile::AddVersion(VersionInfo &newVersion)
{
	// Remove files of new update from older version infos
	auto &versions = GetVersions();
	for(uint32_t i=static_cast<uint32_t>(versions.size()) -1;i!=(uint32_t)-1;i--)
	{
		VersionInfo &info = versions[i];
		for(uint32_t j=static_cast<uint32_t>(info.files.size()) -1;j!=(uint32_t)-1;j--)
		{
			for(uint32_t k=0;k<newVersion.files.size();k++)
			{
				if(info.files[j] == newVersion.files[k])
				{
					info.files.erase(info.files.begin() +j);
					break;
				}
			}
		}
		if(info.files.empty())
			versions.erase(versions.begin() +i);
	}
	versions.push_front(newVersion);
}

void uva::ArchiveFile::Close()
{
	if(m_in != nullptr)
		m_in = nullptr;
	if(m_out != nullptr)
		m_out = nullptr;
}

bool uva::ArchiveFile::GetLatestVersion(util::Version *version)
{
	if(m_versions.empty())
		return false;
	auto &info = m_versions.front();
	*version = info.version;
	return true;
}

uva::ArchiveFile::FileIndexInfo &uva::ArchiveFile::GetRoot() {return *m_root;}
std::deque<VersionInfo> &uva::ArchiveFile::GetVersions() {return m_versions;}

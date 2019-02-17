/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UVA_ARCHIVE_FILE_HPP__
#define __UVA_ARCHIVE_FILE_HPP__

#include <string>
#include <fsys/filesystem.h>
#include <deque>
#include <sharedutils/util_version.h>
#include "uva_fileinfo.hpp"
#include "uva_version_info.hpp"
#include "uva_definitions.hpp"

namespace uva
{
	class DLLUVA ArchiveFile
	{
	public:
		struct FileIndexInfo
			: public std::enable_shared_from_this<FileIndexInfo>
		{
			uint32_t index = 0;
			std::vector<std::shared_ptr<FileIndexInfo>> children;
			std::weak_ptr<FileIndexInfo> parent;
		};
		enum class UpdateResult : uint32_t
		{
			Success = 0,
			ListFileNotFound,
			NothingToUpdate,
			UnableToCreateArchiveFile,
			VersionDiscrepancy,
			UnableToRemoveTemporaryFiles
		};
		~ArchiveFile();
		static ArchiveFile *Open(
			const std::string &updateFileName,
			const std::function<bool(VFilePtr&)> &readCallback=nullptr,
			const std::function<bool(VFilePtrReal&)> &writeCallback=nullptr
		);
		bool GetLatestVersion(util::Version *version);
		FileIndexInfo &GetRoot();
		std::deque<VersionInfo> &GetVersions();
		void AddVersion(VersionInfo &newVersion);
		bool Export();
		VFilePtr &GetFile();
		void GetUpdateFiles(const util::Version &version,std::vector<uint32_t> &updateFiles) const;

		void ExtractAll(const std::string &outPath) const;
		bool ExtractFile(const std::string &fname,const std::string &outName) const;
		bool ExtractFile(const std::string &fname) const;
		bool ExtractData(const std::string &fname,std::vector<uint8_t> &data) const;
		const std::vector<std::shared_ptr<FileInfo>> &GetFiles() const;
		std::shared_ptr<FileInfo> GetByIndex(uint32_t idx);
		FileIndexInfo *FindFileIndexInfo(FileInfo &fi) const;
		std::string GetFullPath(FileIndexInfo *fii) const;
		FileInfo *AddFile(const std::string &fname,uint32_t &idx);
		FileInfo *AddFile(const std::string &fname);
		FileInfo *FindFile(const std::string &fname) const;
		FileInfo *FindFile(const std::string &fname,uint32_t &idx) const;

		void SearchFiles(const std::string &searchPattern,std::vector<FileInfo*> &results) const;

		static UpdateResult PublishUpdate(
			util::Version &version,
			const std::string &updateListFile,
			const std::string &archiveFile,
			const std::function<bool(VFilePtr&)> &readCallback=nullptr,
			const std::function<bool(VFilePtrReal&)> &writeCallback=nullptr,
			const std::function<void(std::string&,std::string&,std::vector<uint8_t>&)> &dataTranslateCallback=nullptr
		);

		static std::string result_code_to_string(UpdateResult code);
	protected:
#pragma pack(push,1)
		struct FileHeader
		{
			uint64_t fileNameOffset = 0;
			uint32_t flags = 0;
			uint64_t size = 0;
			uint64_t sizeUncompressed = 0;
			uint64_t offset = 0;
			uint32_t crc = 0;
		};
#pragma pack(pop)
		ArchiveFile(
			const std::string &updateFileName,
			VFilePtrReal &f,
			const std::function<bool(VFilePtr&)> &readCallback=nullptr,
			const std::function<bool(VFilePtrReal&)> &writeCallback=nullptr
		);
		VFilePtr m_in;
		VFilePtrReal m_out;
		std::string m_updateFile;
		std::deque<VersionInfo> m_versions;
		std::vector<std::shared_ptr<FileInfo>> m_files;
		std::unordered_map<uint32_t,std::vector<uint32_t>> m_hierarchy;
		std::shared_ptr<FileIndexInfo> m_root = nullptr;
		uint64_t m_inFileStartOffset = 0;
		//std::shared_ptr<FileInfo> m_root = nullptr;
		//std::vector<std::weak_ptr<FileInfo>> m_indexedFiles;
		//uint32_t m_nextIndex = 1;
		std::function<bool(VFilePtr&)> m_fReadCallback = nullptr;
		std::function<bool(VFilePtrReal&)> m_fWriteCallback = nullptr;
		bool Extract(const FileInfo &fi,const std::string &outName) const;
		bool ExtractAndDecompress(const FileInfo &fi,std::vector<uint8_t> &data) const;
		//FileInfo *FindFile(FileInfo *fi,const std::string &fname,P_OS os,uint32_t &idx,uint64_t fnameOffset=0) const;
		static UpdateResult PublishUpdate(
			const std::string &filePath,
			util::Version &version,
			std::vector<PublishInfo> &files,
			const std::string &updateFile,
			const std::function<bool(VFilePtr&)> &readCallback=nullptr,
			const std::function<bool(VFilePtrReal&)> &writeCallback=nullptr,
			const std::function<void(std::string&,std::string&,std::vector<uint8_t>&)> &dataTranslateCallback=nullptr
		);

		bool ReadHeader();
		void ReadVersionLayer();
		void ReadFiles();
		void ReadFileNames();
		void ReadFileHierarchy();
		void ReadFileData(uint64_t startOffset,const FileInfo &fi,std::vector<uint8_t> &data) const;

		void WriteHeader(uint64_t &hdVersionOffset,uint64_t &hdFileOffset,uint64_t &hdFileNameOffset,uint64_t &hdHierarchyOffset,uint64_t &hdDataOffset);
		void WriteVersionLayer();
		void WriteFiles(uint64_t &fileHeaderOffset);
		void WriteFileNames(uint64_t startOffset,uint64_t fileHeaderOffset);
		void WriteFileHierarchy();
		void WriteFileData(uint64_t startOffset,uint64_t fileHeaderOffset);
		void Close();
	};
};

#endif

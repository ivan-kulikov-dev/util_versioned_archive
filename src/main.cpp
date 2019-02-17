/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef UVA_EXE
#include "util_versioned_archive.hpp"
#include "uva_archive_file.hpp"
#include <iostream>

int main(int argc,char *argv[])
{
	util::Version version {0,0,2};
	auto r = uva::ArchiveFile::PublishUpdate(version,"updatelist.txt","versioninfo.dat");
	std::cout<<"Result: "<<uva::ArchiveFile::result_code_to_string(r)<<std::endl;

	//static bool publish(const util::Version &version,std::string list="updatelist.txt",char *err=nullptr,const std::string &updateFile="versioninfo.dat");

	auto f = std::unique_ptr<uva::ArchiveFile>(uva::ArchiveFile::Open("versioninfo.dat"));
	auto &versions = f->GetVersions();
	std::cout<<versions.size()<<" versions:"<<std::endl;
	for(auto &v : versions)
	{
		std::cout<<v.version.ToString()<<std::endl;
		for(auto idx : v.files)
		{
			auto fi = f->GetByIndex(idx);
			std::cout<<"\t"<<idx<<" ("<<((fi != nullptr) ? (fi->name) : "Invalid")<<")"<<std::endl;
			//std::cout<<"\t"<<idx<<" ("<<((fi != nullptr) ? (fi->GetFullPath() +fi->name) : "Invalid")<<")"<<std::endl;
		}
	}

	std::cout<<"\nHierarchy:"<<std::endl;
	auto &root = f->GetRoot();
	std::function<void(uva::ArchiveFile::FileIndexInfo&,const std::string&)> fIterateHierarchy = nullptr;
	fIterateHierarchy = [&f,&fIterateHierarchy](uva::ArchiveFile::FileIndexInfo &fii,const std::string &t) {
		auto &fi = f->GetByIndex(fii.index);
		std::cout<<t<<fii.index<<": "<<fi->name<<" ("<<fi->size<<") ("<<fi->IsDirectory()<<")"<<std::endl;
		for(auto &child : fii.children)
			fIterateHierarchy(*child,t +'\t');
	};
	fIterateHierarchy(root,"\t");
	/*auto &files = f->GetFiles();
	for(auto &f : files)
	{
		std::cout<<f->name<<std::endl;
	}*/
	/*auto &root = f->GetRoot();
	std::function<void(FileInfo&,const std::string&)> fIterateHierarchy = nullptr;
	fIterateHierarchy = [&fIterateHierarchy](FileInfo &fi,const std::string &t) {
		std::cout<<t<<fi.name<<" ("<<fi.size<<")"<<std::endl;
		for(auto &child : fi.children)
			fIterateHierarchy(*child,t +'\t');
	};
	fIterateHierarchy(root,"");*/

	//

	version = {0,0,0};
	std::vector<uint32_t> fileIds;
	for(auto &versionInfo : versions)
	{
		if(version < versionInfo.version)
		{
			fileIds.reserve(fileIds.size() +versionInfo.files.size());
			for(auto idx : versionInfo.files)
				fileIds.push_back(idx);
		}
	}

	std::cout<<"Extracting..."<<std::endl;
	//f->Extract("test_extract");
	f->ExtractFile("models/props/metal_fence01.wmd");
	std::cout<<"All files have been extracted!"<<std::endl;

	for(;;);
	return EXIT_SUCCESS;
}
#endif

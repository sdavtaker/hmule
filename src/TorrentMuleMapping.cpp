/*
 * TorrentMuleMapping.cpp
 *
 *  Created on: Jun 27, 2011
 *      Author: Damian Vicino
 */

#include "Logger.h"
#include "TorrentMuleMapping.h"
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include "OtherFunctions.h"

namespace torrent {

MetadataRelation* CTorrentMuleMapping::UpdateMetadata(CMD4Hash* muleId, libtorrent::sha1_hash* BTId, boost::filesystem::path* torrentPath){
	MuleIdToMetadataRelation::iterator muleIt;
	BTIdToMetadataRelation::iterator BTIt;
	InfoFileToMetadataRelation::iterator fileIt;
	MetadataRelation* data = NULL;
	// Check what needs an insert, since missed data will be set as NULL and only NULL data should be updated.
	bool insertMule = (muleId != NULL);
	bool insertBT = (BTId != NULL);
	bool insertFile = (torrentPath != NULL);
	if (muleId != NULL && (m_MuleIHDictionary.end() != (muleIt = m_MuleIHDictionary.find(*muleId)))){
		data = muleIt->second;
		insertMule = false;
	}
	if (BTId !=NULL && (m_BTIHDictionary.end() != (BTIt = m_BTIHDictionary.find(*BTId)))){
		data = BTIt->second;
		insertBT = false;
	}
	if (torrentPath != NULL && (m_BTFileDictionary.end() != (fileIt = m_BTFileDictionary.find(*torrentPath)))){
		data = fileIt->second;
		insertFile =  false;
	}
	// If no old metada was found for update, create a new relation.
	if (data == NULL){
		data = new MetadataRelation(NULL, NULL, NULL, downloading);
	}
	// Create objects for the needed tuple components.
	if (data->get<0>() == NULL){
		if(muleId == NULL) data->get<0>() = NULL;
		else data->get<0>() = new CMD4Hash(*muleId);
	}
	if (data->get<1>() == NULL) {
		if(BTId == NULL) data->get<1>() = NULL;
		else data->get<1>() = new libtorrent::sha1_hash(BTId->to_string());
	}
	if (data->get<2>() == NULL) {
		if(torrentPath == NULL) data->get<2>() = NULL;
		else data->get<2>() = new boost::filesystem::path(*torrentPath);
	}
	// Index the metadata relation
	if (insertMule) m_MuleIHDictionary.insert(MuleIdToMetadataRelation::value_type(*muleId, data));
	if (insertBT) m_BTIHDictionary.insert(BTIdToMetadataRelation::value_type(*BTId, data));
	if (insertFile) m_BTFileDictionary.insert(InfoFileToMetadataRelation::value_type(*torrentPath, data));
	// If metadata is new add it to the iterableMetadataVector for iteration.
	if (insertMule || insertBT || insertFile) m_IterableMetadataRelations.push_back(data);
	// Return the new/updated metadata
	return data;
}

MetadataRelation* CTorrentMuleMapping::UpdateMetadata(CMD4Hash muleId, libtorrent::sha1_hash BTId, boost::filesystem::path torrentPath){
	CMD4Hash * mule_p = &muleId;
	libtorrent::sha1_hash * BTId_p = &BTId;
	boost::filesystem::path * torrentPath_p = &torrentPath;
	return UpdateMetadata(mule_p, BTId_p, torrentPath_p);
}

MetadataRelation* CTorrentMuleMapping::UpdateMetadata(CMD4Hash muleId, libtorrent::sha1_hash BTId){
	CMD4Hash * mule_p = &muleId;
	libtorrent::sha1_hash * BTId_p = &BTId;
	boost::filesystem::path * torrentPath_p = NULL;
	return UpdateMetadata(mule_p, BTId_p, torrentPath_p);
}

MetadataRelation* CTorrentMuleMapping::UpdateMetadata(libtorrent::sha1_hash BTId, boost::filesystem::path torrentPath){
	CMD4Hash * mule_p = NULL;
	libtorrent::sha1_hash * BTId_p = &BTId;
	boost::filesystem::path * torrentPath_p = &torrentPath;
	return UpdateMetadata(mule_p, BTId_p, torrentPath_p);
}

void CTorrentMuleMapping::Erase(CMD4Hash muleId){
	MuleIdToMetadataRelation::iterator muleIt;
	BTIdToMetadataRelation::iterator BTIt;
	InfoFileToMetadataRelation::iterator fileIt;
	MetadataRelation* data = NULL;
	std::vector<MetadataRelation*>::iterator vit;
	
	if (m_MuleIHDictionary.end() != (muleIt = m_MuleIHDictionary.find(muleId))){
		data = muleIt->second;
		if (data->get<1>() !=NULL && (m_BTIHDictionary.end() != (BTIt = m_BTIHDictionary.find(*data->get<1>())))){
			m_BTIHDictionary.erase(BTIt);
		}
		if (data->get<2>() != NULL && (m_BTFileDictionary.end() != (fileIt = m_BTFileDictionary.find(*data->get<2>())))){
			m_BTFileDictionary.erase(fileIt);
		}
		m_MuleIHDictionary.erase(muleIt);
		
		for(vit=m_IterableMetadataRelations.begin(); vit!=m_IterableMetadataRelations.end(); ++vit){
			*vit = data;
			m_IterableMetadataRelations.erase(vit);
			delete data;
		}
	}
}

void CTorrentMuleMapping::SetDownloading(CMD4Hash muleId){
	(m_MuleIHDictionary.at(muleId))->get<3>() = downloading;
}

void CTorrentMuleMapping::SetDownloading(libtorrent::sha1_hash torrentId){
	(m_BTIHDictionary.at(torrentId))->get<3>() = downloading;
}

void CTorrentMuleMapping::SetSharing(CMD4Hash muleId){
	(m_MuleIHDictionary.at(muleId))->get<3>() = sharing;
}

void CTorrentMuleMapping::SetSharing(libtorrent::sha1_hash torrentId){
	(m_BTIHDictionary.at(torrentId))->get<3>() = sharing;
}

void CTorrentMuleMapping::SetRemoved(CMD4Hash muleId){
	(m_MuleIHDictionary.at(muleId))->get<3>() = removed;
}

void CTorrentMuleMapping::SetRemoved(libtorrent::sha1_hash torrentId){
	(m_BTIHDictionary.at(torrentId))->get<3>() = removed;
}

bool CTorrentMuleMapping::IsDownloading(CMD4Hash muleId){
	if (HasMuleIH(muleId)) return downloading == (m_MuleIHDictionary.at(muleId))->get<3>();
	else return false;
}

bool CTorrentMuleMapping::IsDownloading(libtorrent::sha1_hash torrentId){
	if(HasBTIH(torrentId)) return downloading == (m_BTIHDictionary.at(torrentId))->get<3>();
	else return false;
}

bool CTorrentMuleMapping::IsDownloading(boost::filesystem::path& torrentPath){
	if (HasTorrentPath(torrentPath)) return downloading == (m_BTFileDictionary.at(torrentPath))->get<3>();
	else return false;
}

bool CTorrentMuleMapping::IsSharing(CMD4Hash muleId){
	if (HasMuleIH(muleId)) return sharing == (m_MuleIHDictionary.at(muleId))->get<3>();
	else return false;
}

bool CTorrentMuleMapping::IsSharing(libtorrent::sha1_hash torrentId){
	if (HasBTIH(torrentId)) return sharing == (m_BTIHDictionary.at(torrentId))->get<3>();
	else return false;
}

bool CTorrentMuleMapping::IsSharing(boost::filesystem::path& torrentPath){
	if (HasTorrentPath(torrentPath)) return sharing == (m_BTFileDictionary.at(torrentPath))->get<3>();
	else return false;
}

bool CTorrentMuleMapping::WasRemoved(boost::filesystem::path& torrentPath){
	if (HasTorrentPath(torrentPath)) return removed== (m_BTFileDictionary.at(torrentPath))->get<3>();
	else return false;
}

const boost::filesystem::path& CTorrentMuleMapping::GetTorrentPath(CMD4Hash muleId) const {
	return *(m_MuleIHDictionary.at(muleId)->get<2>());
}

const boost::filesystem::path& CTorrentMuleMapping::GetTorrentPath(libtorrent::sha1_hash btId) const {
	return *(m_BTIHDictionary.at(btId)->get<2>());
}

const libtorrent::sha1_hash& CTorrentMuleMapping::GetBTIH(CMD4Hash muleId) const {
	return *(m_MuleIHDictionary.at(muleId)->get<1>());
}

const libtorrent::sha1_hash& CTorrentMuleMapping::GetBTIH(boost::filesystem::path torrentPath) const {
	return *(m_BTFileDictionary.at(torrentPath)->get<1>());
}

const CMD4Hash& CTorrentMuleMapping::GetMuleIH(libtorrent::sha1_hash btId) const {
	return *(m_BTIHDictionary.at(btId)->get<0>());
}

const CMD4Hash& CTorrentMuleMapping::GetMuleIH(boost::filesystem::path torrentPath) const {
	return *(m_BTFileDictionary.at(torrentPath)->get<0>());
}

CTorrentMuleMapping::const_iterator CTorrentMuleMapping::begin() const{
	return m_IterableMetadataRelations.begin();
}

CTorrentMuleMapping::const_iterator CTorrentMuleMapping::end() const{
	return m_IterableMetadataRelations.end();
}

void CTorrentMuleMapping::Load(boost::filesystem::path workingDir){
	int counter=0;
	int lineCounter=0;
	// Check if MBTD.dat is accessible
	if (!boost::filesystem::exists(workingDir / "MBTD.dat")){
		AddLogLineCS(_("Metadata dictionaries for BT doesn't exist."));
	} else {
		std::string input; // MBTD.dat reading buffer
		std::vector<std::string> parsedLine; // parsed line of MBTD.dat
		CMD4Hash muleId;
		libtorrent::sha1_hash BTId;
		boost::filesystem::path torrentPath;
		state downloadingState;
		MetadataRelation* data;
		boost::filesystem::ifstream MBTD( workingDir / "MBTD.dat");
		while(MBTD.good()){
			lineCounter++;
			// Obtain a config line
			std::getline(MBTD, input);
			// If line is empty, skip it.
			if (input.length() == 0) continue;
			// If line is commented, skip it.
			if (input.at(0) == '#') continue;
			// Clean up previous parsing.
			parsedLine.clear();
			data = NULL;
			try {
				boost::split(parsedLine, input, boost::is_any_of(":"));
			} catch (int e){
				AddDebugLogLineC(logTorrent, wxT("Exception catched in split, review the MBTD.dat file"));
				AddLogLineCS(_("Corrupted MBTD.dat file in line: ") + boost::lexical_cast<std::string>(lineCounter));
				continue;
			}
			// Check the parsed data has enough info, last (state) field is mandatory.
			if(parsedLine.size() == 4 && parsedLine[3].length() != 0){
				if (parsedLine[0].length()) muleId.Decode(parsedLine[0]);
				if (parsedLine[1].length()) BTId.assign(libtorrent::base32decode(parsedLine[1]));
				if (parsedLine[2].length()) torrentPath = boost::filesystem::path(parsedLine[2]);
				downloadingState = (state) boost::lexical_cast<int>(parsedLine[3]);
				// Choose what update method to use based in the loaded data.
				if (parsedLine[0].length() && parsedLine[1].length() && parsedLine[2].length()){
					// Whole relation is known
					data = UpdateMetadata(muleId, BTId, torrentPath);
				}
				else if (!parsedLine[0].length() && parsedLine[1].length() && parsedLine[2].length()){
					// There is no info about mule MD4 identifier yet, only torrent downloading.
					data = UpdateMetadata(BTId, torrentPath);
				}
				else if (parsedLine[0].length() && parsedLine[1].length() && !parsedLine[2].length()){
					// Metadata for torrent is not known, but SHA1 identifier was already transfered.
					data = UpdateMetadata(muleId, BTId);
				}
				if (data !=  NULL){
					counter++; // Increase the count of loaded metadata relations.
					data->get<3>() = downloadingState; // Set the downloading state
				}
			} else {
				AddLogLineCS(_("Corrupted MBTD.dat file in line: ") + boost::lexical_cast<std::string>(lineCounter));
				AddDebugLogLineC(logTorrent, wxT("Reading line from MBTD.dat with wrong count of fields in line: ") + boost::lexical_cast<std::string>(lineCounter) +
					" saying: " + input);
			}
		}
		MBTD.close();
		AddLogLineNS(_("MBTD.dat - Loaded definitions: ") + (boost::lexical_cast<std::string>(counter)));
	}
}

void CTorrentMuleMapping::Save(boost::filesystem::path workingDir){
	// This method fully rewrites the MBTD.dat file
	int counter=0;
	boost::filesystem::ofstream MBTD( workingDir / "MBTD.dat", std::ios::trunc);
	MBTD << "#MuleToBTDictFile" << std::endl;
	for ( const_iterator it = begin(); it != end(); it++){
		if ((*it)->get<0>()) MBTD << (*it)->get<0>()->EncodeSTL();
		MBTD << ":";
		if ((*it)->get<1>()) MBTD << libtorrent::base32encode((*it)->get<1>()->to_string());
		MBTD << ":";
		if ((*it)->get<2>()) MBTD << (*it)->get<2>()->native();
		MBTD << ":";
		MBTD << (*it)->get<3>(); //state
		MBTD << std::endl;
		counter++;
	}
	MBTD.close();
	AddLogLineNS(_("MBTD.dat - Saved definitions: ") + (boost::lexical_cast<std::string>(counter)));
}

bool CTorrentMuleMapping::HasTorrentPath(CMD4Hash muleId) const{
	MuleIdToMetadataRelation::const_iterator it  = m_MuleIHDictionary.find(muleId);
	if (m_MuleIHDictionary.end() != it) {
		return (it->second->get<2>() != NULL);
	} else {
		return false;
	}
}

bool CTorrentMuleMapping::HasTorrentPath(libtorrent::sha1_hash btId) const{
	BTIdToMetadataRelation::const_iterator it  = m_BTIHDictionary.find(btId);
	if (m_BTIHDictionary.end() != it) {
		return (it->second->get<2>() != NULL);
	} else {
		return false;
	}
}

bool CTorrentMuleMapping::HasTorrentPath(boost::filesystem::path torrentPath) const{
	InfoFileToMetadataRelation::const_iterator it  = m_BTFileDictionary.find(torrentPath);
	return (m_BTFileDictionary.end() != it);
}

bool CTorrentMuleMapping::HasBTIH(CMD4Hash muleId) const{
	MuleIdToMetadataRelation::const_iterator it  = m_MuleIHDictionary.find(muleId);
	if (m_MuleIHDictionary.end() != it) {
		return (it->second->get<1>() != NULL);
	} else {
		return false;
	}
}

bool CTorrentMuleMapping::HasBTIH(libtorrent::sha1_hash btId) const{
	BTIdToMetadataRelation::const_iterator it  = m_BTIHDictionary.find(btId);
	return (m_BTIHDictionary.end() != it);
}

bool CTorrentMuleMapping::HasBTIH(boost::filesystem::path torrentPath) const{
	InfoFileToMetadataRelation::const_iterator it  = m_BTFileDictionary.find(torrentPath);
	if (m_BTFileDictionary.end() != it) {
		return (it->second->get<1>() != NULL);
	} else {
		return false;
	}
}

bool CTorrentMuleMapping::HasMuleIH(CMD4Hash muleId) const{
	MuleIdToMetadataRelation::const_iterator it  = m_MuleIHDictionary.find(muleId);
	return (m_MuleIHDictionary.end() != it);
}

bool CTorrentMuleMapping::HasMuleIH(libtorrent::sha1_hash btId) const{
	BTIdToMetadataRelation::const_iterator it  = m_BTIHDictionary.find(btId);
	if (m_BTIHDictionary.end() != it) {
		return (it->second->get<0>() != NULL);
	} else {
		return false;
	}
}

bool CTorrentMuleMapping::HasMuleIH(boost::filesystem::path torrentPath) const{
	InfoFileToMetadataRelation::const_iterator it  = m_BTFileDictionary.find(torrentPath);
	if (m_BTFileDictionary.end() != it) {
		return (it->second->get<0>() != NULL);
	} else {
		return false;
	}
}

CTorrentMuleMapping::~CTorrentMuleMapping() {
	for(std::vector<MetadataRelation*>::iterator it=m_IterableMetadataRelations.begin(); it != m_IterableMetadataRelations.end(); ++it){
		if (NULL!= (*it)->get<0>()) delete((*it)->get<0>());
		if (NULL!= (*it)->get<1>()) delete((*it)->get<1>());
		if (NULL!= (*it)->get<2>()) delete((*it)->get<2>());
	}
	m_IterableMetadataRelations.clear();
	m_MuleIHDictionary.clear();
	m_BTIHDictionary.clear();
	m_BTFileDictionary.clear();
}

}



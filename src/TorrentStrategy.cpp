/*
 * TorrentStrategy.cpp
 *
 *  Created on: Jul 26, 2011
 *      Author: Damian Vicino
 */

#include "TorrentStrategy.h"
#include "Constants.h"
#include "Logger.h"
#include "GetTickCount.h"
#include "Torrent.h"
#include "PartFile.h"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <libtorrent/peer_info.hpp>
#include <common/Macros.h>
#include "amule.h"


namespace torrent {

CTorrentStrategy::CTorrentStrategy(libtorrent::session* torrentSession, CTorrentMuleMapping* mapping) {
	m_ts = torrentSession;
	m_lastTimeProcessWasRun = ::GetTickCount();
	m_lastTimeAlertsWereProcessed = ::GetTickCount();
	m_tmm = mapping;
}

uint64 CTorrentStrategy::GetCompletedSize(CMD4Hash fileId){
//TODO: To do a fine display of stats in UI, an stimation of the downloaded data should take in account both protocols.
}

void CTorrentStrategy::OnReceivedMetadata(libtorrent::metadata_received_alert* mra){
		libtorrent::torrent_handle h = mra->handle;
		libtorrent::create_torrent ct(h.get_torrent_info());
		boost::filesystem::path filename(h.name());
		m_tmm->UpdateMetadata(h.info_hash(), boost::filesystem::path(CTorrent::GetInstance().SaveTorrent(ct,filename)));
		m_tmm->SetDownloading(h.info_hash());
}

CTorrentStrategy::CTorrentStrategy() {} //! for inheritance purposes only.
CTorrentStrategy::~CTorrentStrategy() {}

CTorrentAlwaysFallToBTStrategy::CTorrentAlwaysFallToBTStrategy(libtorrent::session* torrentSession, CTorrentMuleMapping* mapping):CTorrentStrategy(torrentSession, mapping){}

CTorrentAlwaysFallToBTStrategy::~CTorrentAlwaysFallToBTStrategy(){

}

void CTorrentStrategy::OnFinishedDownload(libtorrent::torrent_finished_alert* tfa ){
	libtorrent::torrent_handle h = tfa->handle;
	if (m_tmm->HasBTIH(h.info_hash()) && m_tmm->IsSharing(h.info_hash())) return; // Skip files that were already finished in antoher session
	if (m_tmm->HasMuleIH(h.info_hash())){
		CPartFile* mulePartFile = theApp->downloadqueue->GetFileByID(m_tmm->GetMuleIH(h.info_hash()));
		if (NULL != mulePartFile){
			mulePartFile->Delete();
		}
	}
	h.move_storage(std::string(thePrefs::GetIncomingDir().GetPrintable()));
}

void CTorrentStrategy::ProcessAlert(std::auto_ptr<libtorrent::alert> a){
	if (a.get()->category() == libtorrent::alert::error_notification) {
		AddLogLineCS(_("Torrent error: ") + (a.get()->message()) + wxT(" - ") + std::string(a.get()->what()));
	}
	if (libtorrent::torrent_finished_alert* p = libtorrent::alert_cast<libtorrent::torrent_finished_alert>(a.get())){
		AddDebugLogLineN(logTorrent, wxT("alert type: FINISHED ALERT"));
		OnFinishedDownload(p);
	} else if (libtorrent::storage_moved_alert* sma = libtorrent::alert_cast<libtorrent::storage_moved_alert>(a.get())){
		AddDebugLogLineN(logTorrent, wxT("alert type: STORAGE MOVED ALERT"));
		libtorrent::torrent_handle h = sma->handle;
		theApp->sharedfiles->Reload();
		if(m_tmm->HasMuleIH(h.info_hash())){
			m_validationQueue.push_back(pair<CMD4Hash, int>( m_tmm->GetMuleIH(h.info_hash()), 0));
		}
		if(h.get_torrent_info().num_files() != 1){
			AddLogLineNS("Downloaded a multi-file content, it will not be shared in aMule network");
			//TODO: Something need to map the amulecollections to multi file torrents to avoid this message
		}
	} else if (libtorrent::torrent_resumed_alert* tra = libtorrent::alert_cast<libtorrent::torrent_resumed_alert>(a.get())){
		AddDebugLogLineN(logTorrent, wxT("alert type: TORRENT RESUMED ALERT"));
		OnTorrentResumed(tra);
	} else if (libtorrent::metadata_received_alert* mra = libtorrent::alert_cast<libtorrent::metadata_received_alert>(a.get())){
		AddDebugLogLineN(logTorrent, wxT("alert type: METADATA RECEIVED ALERT"));
		OnReceivedMetadata(mra);
	}
}

void CTorrentAlwaysFallToBTStrategy::Process(){
	// Process alerts
	uint32 tick = ::GetTickCount();
	if(!( tick - m_lastTimeAlertsWereProcessed < SEC2MS(1))) {
		std::auto_ptr<libtorrent::alert> a = m_ts->pop_alert();
		if (a.get() != NULL) ProcessAlert(a);
		m_lastTimeAlertsWereProcessed = tick;
		if (!m_validationQueue.empty() && (tick - m_lastTimeValidationQueueProcessed < SEC2MS(5))){
			ValidateDownloads();
		}
		m_lastTimeValidationQueueProcessed = tick;
	}
}

void CTorrentStrategy::ValidateDownloads(){
	CKnownFile *file ;
	std::vector<pair<CMD4Hash, int> >::iterator it = m_validationQueue.begin();
	if (theApp->sharedfiles->GetCount() && (NULL != (file =  theApp->sharedfiles->GetFileByID(it->first)))){
		if (it->second > MAX_ALLOWED_VALIDATION_TRIES){
			m_validationQueue.erase(it);
			AddLogLineCS(_("A file couldn't be validated against MD4 after download"));
			//TODO: This file was completed success in torrent and the MD4 doesn't match, someone injected a fake SHA1, handle it.
		}
		it->second++;
	} else {
		AddDebugLogLineN(logTorrent, wxT("Validated downloaded file against its MD4 in try: ") + boost::lexical_cast<std::string>(it->second));
		m_validationQueue.erase(it);
	}	
}

void CTorrentStrategy::OnTorrentResumed(libtorrent::torrent_resumed_alert* ){ }

void CTorrentAlwaysFallToBTStrategy::OnTorrentResumed(libtorrent::torrent_resumed_alert* tra ){
	libtorrent::torrent_handle h = tra->handle;
	if (h.is_finished() || !h.has_metadata()) return; // Skip processing resume alerts on shares or non-metadata transfered downloads
	if(m_tmm->HasMuleIH(h.info_hash()) && theApp->downloadqueue->IsFileExisting(m_tmm->GetMuleIH(h.info_hash()))){
		CPartFile* part = theApp->downloadqueue->GetFileByID(m_tmm->GetMuleIH(h.info_hash()));
		if (part != NULL){
			part->PauseFile();
			AddLogLineNS("Paused mule transfer because of AlwaysFallToTorrent Strategy, downloading in background useing torrent protocol.");
		} else {
			AddDebugLogLineC(logTorrent, wxT("It was requested to pause a NULL part file."));
		}
	}
}


CNoTorrentStrategy::CNoTorrentStrategy(libtorrent::session* torrentSession, CTorrentMuleMapping* mapping):CTorrentStrategy(torrentSession, mapping){}
void CNoTorrentStrategy::Process(){
	// Alerts need to be processed just in case we receive some metadata and want to save it.
	uint32 tick = ::GetTickCount();
	if( tick - m_lastTimeAlertsWereProcessed > SEC2MS(1)) return;
	std::auto_ptr<libtorrent::alert> a = m_ts->pop_alert();
	if (a.get() != NULL){
		ProcessAlert(a);
	}
	m_lastTimeAlertsWereProcessed = tick;
}

void CNoTorrentStrategy::OnReceivedMetadata(libtorrent::metadata_received_alert* mra){
		libtorrent::torrent_handle h = mra->handle;
		libtorrent::create_torrent ct(h.get_torrent_info());
		boost::filesystem::path filename(h.name());
		m_tmm->UpdateMetadata(h.info_hash(), boost::filesystem::path(CTorrent::GetInstance().SaveTorrent(ct,filename)));
		m_tmm->SetDownloading(h.info_hash());
		//only provide the torrent metadata, dont download it.
		h.auto_managed(false);
		h.set_upload_mode(true); 
		AddDebugLogLineN(logTorrent, wxT("No-Torrent is suspending download!"));
}
CNoTorrentStrategy::~CNoTorrentStrategy(){}


CTorrentSwitchToTheMostUsablePeersStrategy::CTorrentSwitchToTheMostUsablePeersStrategy(libtorrent::session* torrentSession, CTorrentMuleMapping* mapping):CTorrentStrategy(torrentSession, mapping){}

void CTorrentSwitchToTheMostUsablePeersStrategy::Process(){
	uint32 tick = ::GetTickCount();
	if(!( tick - m_lastTimeAlertsWereProcessed < SEC2MS(1))) {
		std::auto_ptr<libtorrent::alert> a = m_ts->pop_alert();
		if (a.get() != NULL) ProcessAlert(a);
		m_lastTimeAlertsWereProcessed = tick;
		if (!m_validationQueue.empty() && (tick - m_lastTimeValidationQueueProcessed < SEC2MS(5))){
			ValidateDownloads();
		}
		m_lastTimeValidationQueueProcessed = tick;
	}
	// If 60 seconds passed since last optimistic resume, do it!
	if ( tick - m_lastTimeOptimisticResume > SEC2MS(60)){
		AddLogLineNS(wxT("DEBUG: Strategy going Optimistic resume."));
		// Resuming downloads in Mule Download Queue
		for (std::vector<CMD4Hash>::iterator muleIt = m_pausedMule.begin(); muleIt != m_pausedMule.end(); ++muleIt){
			CPartFile * part = theApp->downloadqueue->GetFileByID(*muleIt);
			if (part != NULL){
				part->ResumeFile();
			}
		}
		m_pausedMule.clear();
		// Resuming downloads in Torrent Session
		for (std::vector<libtorrent::sha1_hash>::iterator torrentIt = m_pausedTorrent.begin(); torrentIt != m_pausedTorrent.end(); ++torrentIt){
			libtorrent::torrent_handle th = m_ts->find_torrent(*torrentIt);
			if (th.is_valid()){
				th.set_upload_mode(false);
				th.auto_managed(true);
			}
		}
		m_pausedTorrent.clear();
		m_lastTimeOptimisticResume = tick;
	}	
	
	//TODO: do a lock-safe iteration for the pauses.
	/*if ( tick - m_lastTimeOptimisticResume > SEC2MS(10) && tick - m_lastTimeAppliedPauses > SEC2MS(60)){
		AddLogLineNS("DEBUG: Pausing the lower tempting download items in each queue");
		for (CTorrentMuleMapping::const_iterator tmmit = m_tmm->begin(); tmmit != m_tmm->end(); ++m_tmm){
			if ( (*tmmit)->get<0>() != NULL  && (*tmmit)->get<1>() != NULL && (*tmmit)->get<3>() == downloading){
				CPartFile* part = theApp->downloadqueue->GetFileByID(*(*tmmit)->get<0>());
				if (part != NULL) {
					libtorrent::torrent_handle th = m_ts->find_torrent(*(*tmmit)->get<1>());
					if (th.is_valid()){
						AddLogLineNS(boost::lexical_cast<std::string>(part->GetKBpsDown()).append("<?").append(boost::lexical_cast<std::string>(th.status().download_rate)));
						if (part->GetKBpsDown() < th.status().download_rate){
							part->PauseFile();
							m_pausedMule.push_back(*(*tmmit)->get<0>());
						} else {
							th.auto_managed(false);
							th.set_upload_mode(true);
							m_pausedTorrent.push_back(*(*tmmit)->get<1>());
						}
					}
				}
			}
		}
		m_lastTimeAppliedPauses = tick;
	}*/
}

void CTorrentStrategy::GiveUp(CMD4Hash fileId){ }

void CTorrentSwitchToTheMostUsablePeersStrategy::GiveUp(CMD4Hash fileId){
	if (m_tmm->HasBTIH(fileId)){
		libtorrent::sha1_hash BTIH = m_tmm->GetBTIH(fileId);
		for (std::vector<libtorrent::sha1_hash>::iterator torrentIt = m_pausedTorrent.begin(); torrentIt != m_pausedTorrent.end(); ++torrentIt){
			if ( *torrentIt == BTIH){
				m_pausedTorrent.erase(torrentIt);
				break;
			}
		}
	}
}

void CTorrentSwitchToTheMostUsablePeersStrategy::OnFinishedDownload(libtorrent::torrent_finished_alert* tfa ){
	// Resuming downloads in Torrent Session
	for (std::vector<libtorrent::sha1_hash>::iterator torrentIt = m_pausedTorrent.begin(); torrentIt != m_pausedTorrent.end(); ++torrentIt){
		if ( *torrentIt == tfa->handle.info_hash()){
			m_pausedTorrent.erase(torrentIt);
			break;
		}
	}
	CTorrentStrategy::OnFinishedDownload(tfa);
}


CTorrentSwitchToTheMostUsablePeersStrategy::~CTorrentSwitchToTheMostUsablePeersStrategy(){}

} /* namespace torrent */

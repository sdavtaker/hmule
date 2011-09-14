/*
 * Torrent.cpp
 *
 *  Created on: Jun 10, 2011
 *      Author: Damian Vicino
 */

#include "Torrent.h"
#include "Logger.h"
#include <string>
#include <boost/asio.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lambda/lambda.hpp>
#include <cstdlib>
#include <libtorrent/escape_string.hpp>
#include <iostream>
#include <iterator>
#include "Preferences.h"
#include "OtherFunctions.h"
#include <libtorrent/magnet_uri.hpp>


namespace torrent {
typedef boost::asio::ip::basic_endpoint<boost::asio::ip::tcp> endpoint; // Listener for bt incomming connections

CTorrent::CTorrent() {
	m_strategy = NULL;
}

CTorrent& CTorrent::GetInstance(){
	static CTorrent instance;
	return instance;
}

void CTorrent::StartTorrentSession() {
	// Version need to be registered in libtorrent-rasterbar so other clients don't use same identifier, AM 2.9.1.0 in test for now.
	m_ts = new libtorrent::session(libtorrent::fingerprint("AM", '2', '9', '1', '0'), libtorrent::session::start_default_features
                        | libtorrent::session::add_default_plugins, libtorrent::alert::error_notification | libtorrent::alert::status_notification | libtorrent::alert::storage_notification);
	// Open listener port, it starts trying with 7000, goes all way until 8000 until find an bind then starts in 1.
	m_ts->listen_on(std::pair<int, int>(7000, 8000));
	AddLogLineNS(_("Started libtorrent session in port: ") + (boost::lexical_cast<std::string>(GetPort())));
	boost::filesystem::path osDir = CPathToBoost(thePrefs::GetOSDir());
	// Load previous session metadata, lt-data.dat has the content of session state, stats, etc, but no torrent metadata.
	boost::filesystem::path fullpathState = boost::filesystem::system_complete( osDir / "lt-state.dat");
	if (boost::filesystem::exists(fullpathState)){
		//TODO: implement the use of session resume and torrents fast resume, session is already been saved ok.
		//boost::filesystem::ifstream stateStream(fullpathState);
		//m_ts->load_state(libtorrent::bdecode(std::istream_iterator<char>(stateStream), std::istream_iterator<char>()));
		//AddLogLineN(_("Loaded torrent previous session state"));
	}
	// Load previous known metadata relations.
	m_tmm.Load(osDir);
	AddLogLineNS(_("Loaded mule-torrent relation dictionaries"));
	// Load torrent metadata from known info-files registered in the loaded metadata relations
	for (CTorrentMuleMapping::const_iterator it = m_tmm.begin(); it != m_tmm.end(); ++it){
		if ( (*it)->get<2>() != NULL ){
			LoadMetadataFile(*(*it)->get<2>());
		}
	}
	AddLogLineNS(_("Loaded known torrent files"));
	SetStrategy(thePrefs::GetTorrentStrategy());
}

const int CTorrent::GetPort() const{
	return CTorrent::m_ts->listen_port();
}

std::string CTorrent::GetBTIHAsString(const CMD4Hash fileId){
	return libtorrent::base32encode(m_tmm.GetBTIH(fileId).to_string());
}

void CTorrent::EndTorrentSession() {
	// Pause all downloads to save non-corrupted state.
	m_ts->pause();
	libtorrent::entry session_state;
	// Save Torrent session state in lt-state.dat.
	m_ts->save_state(session_state);
	std::vector<char> state_v;
	libtorrent::bencode(back_inserter(state_v), session_state);
	boost::filesystem::path osDir = CPathToBoost(thePrefs::GetOSDir());
	boost::filesystem::path fullpathState = boost::filesystem::system_complete( osDir / "lt-state.dat");
	boost::filesystem::ofstream savingState(fullpathState, std::ofstream::binary);
	savingState << std::string(state_v.begin(), state_v.end());
	savingState.close();
	// Save transfered torrent metadata that was not previously saved.
	for (CTorrentMuleMapping::const_iterator MTBTit = m_tmm.begin(); MTBTit != m_tmm.end(); ++MTBTit){
		if((*MTBTit)->get<1>() && !(*MTBTit)->get<2>()){
			libtorrent::torrent_handle th;
			if ((*MTBTit)->get<1>() != NULL){
				th = m_ts->find_torrent(*(*MTBTit)->get<1>());
				if(th.has_metadata()) {
					libtorrent::create_torrent ct(th.get_torrent_info());
					boost::filesystem::path filename(th.name());
					m_tmm.UpdateMetadata(*(*MTBTit)->get<1>(), boost::filesystem::path(SaveTorrent(ct, filename)));
				}
			}
		}
	}
	// Save metadata relations.
	m_tmm.Save(osDir);
	// Delete Torrent Session.
	delete m_ts;
	m_ts = NULL;
	AddLogLineNS(_("Finished Torrent session"));
}

bool CTorrent::HasBTMetadata(const CMD4Hash fileId) const{
	return (m_tmm.HasTorrentPath(fileId));
}

libtorrent::torrent_handle& CTorrent::LoadMetadataFile(boost::filesystem::path& filename){
	libtorrent::torrent_handle th;
	try {
		boost::filesystem::path tempDir = CPathToBoost(thePrefs::GetTempDir());
		boost::filesystem::path torrentDir = CPathToBoost(thePrefs::GetTorrentDir());
		boost::filesystem::path incommingDir = CPathToBoost(thePrefs::GetIncomingDir());
		if (boost::filesystem::exists(torrentDir)){ // Can't read non created directories.
			AddDebugLogLineN(logTorrent, wxT("Loading torrent metadata from: ") + ((torrentDir / filename).native()));
			libtorrent::add_torrent_params p;
			if(m_tmm.HasTorrentPath(filename)){
				if(m_tmm.IsSharing(filename)){
					// If metadata relation says this file is been sharing/seeded, it should be in Incomming directory
					p.save_path = incommingDir.native();
				} else if (m_tmm.IsDownloading(filename)){
					// If metadata relation says this file is been downloading, it should be in Temp directory.
					p.save_path = tempDir.native();
				} else if (m_tmm.WasRemoved(filename)){
					// If metadata relation says this file was removed after downloaded, we only sharing the metadata dictionary.
					p.save_path = tempDir.native();
					p.auto_managed = false;
					p.upload_mode =  true;
				} else {
					// If metadata relation says this file is not sharing, neither downloading, something is really wrong.
					AddLogLineCS(_("Corrupted metadata dictionary, file defined with unknow state."));
					throw 2099;
				}
			} else {
				// If file has no relation metadata yet, it is a new download.
				p.save_path = tempDir.native();
			}
			p.ti = new libtorrent::torrent_info((torrentDir / filename).native());
			try {
				th = m_ts->add_torrent(p);
				if (m_tmm.IsSharing(filename)){
					sharedTorrentsWaitingCheck.push_back(th);
					AddDebugLogLineN(logTorrent, wxT("Got a file in the queue for remove check"));
				}
			} catch(libtorrent::libtorrent_exception &e){
				AddLogLineCS(_("Error loading known torrent from previous session: ") + e.error().message());
			}
		}
	} catch (libtorrent::libtorrent_exception &le) {
		AddLogLineCS(_("Fail to load a torrent metadata file: ") + filename.native());
		std::cerr << le.what() << std::endl;
	} catch (int i) {
		if (i != 2099) throw i;
		AddLogLineCS(_("Corrupted metadata relation for file: ") + filename.native());
	}
	return th;
}

void CTorrent::AddDownloadUsingSHA1AndPeer(const CMD4Hash fileId, std::string SHA1Hash, std::string peerIP, int port){
	AddDebugLogLineN(logTorrent, wxT("Adding torrent metadata request: ") + SHA1Hash + _(" Port:") + peerIP + wxT(":") + boost::lexical_cast<std::string>(port));
	libtorrent::add_torrent_params p;
	libtorrent::torrent_handle th;
	boost::filesystem::path tempDir = CPathToBoost(thePrefs::GetTempDir());
	p.save_path = tempDir.native();
	p.info_hash = libtorrent::sha1_hash(libtorrent::base32decode(SHA1Hash));
	try {
		th = m_ts->find_torrent(p.info_hash);
		if(! th.is_valid()){
			th = m_ts->add_torrent(p);
			//dictionary
			m_tmm.UpdateMetadata(fileId, th.info_hash());
		}
		//add a peer
		endpoint peerConnection(boost::asio::ip::address::from_string(peerIP), port);
		th.connect_peer(peerConnection, 1);
		if (m_ts->is_dht_running()){
			m_ts->add_dht_node(std::pair<std::string,int>(peerIP, port));
		}
	} catch (libtorrent::libtorrent_exception &le) {
		std::cerr << le.what() << std::endl;
	}
	AddDebugLogLineN(logTorrent, wxT("Added BT peer for: ") + (fileId.EncodeSTL()));
}

bool CTorrent::AddDownloadUsingTorrentFile(boost::filesystem::path p){
	boost::filesystem::copy_file(p, CPathToBoost(thePrefs::GetTorrentDir()) / p.filename(), boost::filesystem::copy_option::fail_if_exists);
	LoadUnregisteredTorrents();
	return m_tmm.HasTorrentPath(p.filename());
}


bool CTorrent::AddDownloadFromMagnet(std::string magnet){
	libtorrent::add_torrent_params p;
	p.save_path = CPathToBoost(thePrefs::GetTorrentDir()).native();
	libtorrent::torrent_handle th = libtorrent::add_magnet_uri(*m_ts, magnet, p);
	return th.is_valid();
}

void CTorrent::CreateMetadataForFile(const CMD4Hash fileId, const CPath& filename, const CPath& storeDir){
	CreateMetadataForFile(fileId, CPathToBoost(filename), CPathToBoost(storeDir));
}

void CTorrent::CreateMetadataForFile(const CMD4Hash fileId, boost::filesystem::path filename, boost::filesystem::path storeDir){
	AddLogLineNS(_("Creating BitTorrent metadata for:") + (storeDir/filename).native());
	boost::filesystem::path fullpath = storeDir / filename;
	// Prepare basic flags and info
	int flags = 0;
	int pad_file_limit = -1;
	int piece_size = 0;
	std::string creator_str = std::string(thePrefs::GetUserNick());
	libtorrent::file_storage fs;
	libtorrent::add_files(fs, fullpath.native(), boost::lambda::constant(true), flags);
	if (fs.num_files() == 0) { // Check there is content defined
		AddLogLineCS(_("Creation of Torrent metadata required, but no files provided for: ") + (storeDir /  filename).native());
	}
	libtorrent::create_torrent t(fs, piece_size, pad_file_limit, flags);
	try {
		// Compute all the pieces hashes.
		AddDebugLogLineN(logTorrent, wxT("Creating .torrent for:") + fullpath.branch_path().native());
		libtorrent::set_piece_hashes(t, fullpath.branch_path().native());
	} catch (libtorrent::libtorrent_exception &le) {
		AddLogLineCS(_("Found problems while hashing Torrent metadata: ") + le.what());
	}
	t.set_creator(creator_str.c_str());
	// Save torrent metadata to file.
	boost::filesystem::path filenameTorrent = SaveTorrent(t, filename);
	// Load file created to current session.
	libtorrent::torrent_handle th = LoadMetadataFile(filenameTorrent);
	// Update metadata relation dictionaries.
	m_tmm.UpdateMetadata(fileId, th.info_hash(), filenameTorrent);
	m_tmm.SetSharing(fileId);
}

boost::filesystem::path CTorrent::SaveTorrent(libtorrent::create_torrent & t, boost::filesystem::path & filenameBoosted){
	std::vector<char> torrent;
	bencode(back_inserter(torrent), t.generate());
	boost::filesystem::path filenameTorrent(std::string(filenameBoosted.native()).append(".torrent"));
	boost::filesystem::path torrentDir = CPathToBoost(thePrefs::GetTorrentDir());
	if (!boost::filesystem::exists(torrentDir)){ // If torrent metadata directory doesn't exist, create it.
		boost::filesystem::create_directory(torrentDir);
		AddLogLineN(_("Configured directory for Torrent metadata is created in: ") + (torrentDir.native()));
	}
	boost::filesystem::path fullpathTorrent = boost::filesystem::system_complete( torrentDir / filenameTorrent);
	boost::filesystem::ofstream savingTorrent(fullpathTorrent, std::ofstream::binary);
	savingTorrent << std::string(torrent.begin(), torrent.end());
	savingTorrent.close();
	return filenameTorrent;
}

void CTorrent::LoadUnregisteredTorrents(){
	boost::filesystem::path torrentDir = CPathToBoost(thePrefs::GetTorrentDir());
	if (boost::filesystem::exists(torrentDir)){
		for (boost::filesystem::directory_iterator dit(torrentDir); dit != boost::filesystem::directory_iterator(); ++dit){
			boost::filesystem::path filename(dit->path().filename().native());
			if(!boost::filesystem::is_directory(dit->path()) &&
					(dit->path()).extension().native().compare(std::string(".torrent")) == 0  &&
					!m_tmm.HasTorrentPath(filename)){
				//2 different torrent files with same metadata will collition in the session with BIG EXPLOSION, be careful.
				AddLogLineNS(_("Loaded unregistered torrent: ") + filename.native());
				libtorrent::torrent_handle th = LoadMetadataFile(filename);
				m_tmm.UpdateMetadata(th.info_hash(), filename);
			}
		}
	}
}

void CTorrent::SetStrategy(int strategySelected){
	if (m_strategy) delete m_strategy;
	switch(strategySelected){
	case NO_BT: // Avoid the use of Torrent protocol.
		m_strategy = new CNoTorrentStrategy(m_ts, &m_tmm);
		AddLogLineNS(_("Torrent selected strategy: NO-BT"));
		break;
	default:
	case ALLWAYS_FALL_TO_BT: // If the Torrent Metadata was received, go with Torrent protocol only.
		m_strategy =  new CTorrentAlwaysFallToBTStrategy(m_ts, &m_tmm);
		AddLogLineNS(_("Torrent selected strategy: Always Fall To Torrent"));
		break;
	case SWITCH_TO_THE_MOST_USABLE_PEERS: // Greedily switch to the protocol with more sources.
		m_strategy =  new CTorrentSwitchToTheMostUsablePeersStrategy(m_ts, &m_tmm);
		AddLogLineNS(_("Torrent selected strategy: Switch To The Most Usable Peers Strategy"));
	}
}

void CTorrent::GiveUp(CMD4Hash file_id){
	if (m_tmm.HasMuleIH(file_id)){
		m_strategy->GiveUp(file_id);
		if(m_tmm.HasBTIH(file_id)){
			libtorrent::torrent_handle th = m_ts->find_torrent(m_tmm.GetBTIH(file_id));
			if (th.is_valid()) m_ts->remove_torrent(th, libtorrent::session::delete_files);
		}
		if(m_tmm.HasTorrentPath(file_id)){
			boost::filesystem::path p = CPathToBoost(thePrefs::GetTorrentDir()) / m_tmm.GetTorrentPath(file_id);
			AddLogLineNS(_("Removing torrent file: ") + p.native());
			boost::filesystem::remove(p);
		}
		m_tmm.Erase(file_id);
	}
}

void CTorrent::StartMainline(){
	AddLogLineNS(_("Started Mainline DHT"));
	m_ts->start_dht();
}

void CTorrent::StopMainline(){
	AddLogLineNS(_("Stopped Mainline DHT"));
	m_ts->stop_dht();
}

bool CTorrent::IsMainlineConnected(){
	return m_ts->is_dht_running();
}

void CTorrent::Process(){
	if (sharedTorrentsWaitingCheck.size() == 0){
		m_strategy->Process();
	} else {
		CheckLoadedSharedTorrents();
	}
}

void CTorrent::CheckLoadedSharedTorrents(){
	//checking only 1 item per call to avoid long procedure
	std::vector<libtorrent::torrent_handle>::iterator it = sharedTorrentsWaitingCheck.begin();
	if (it != sharedTorrentsWaitingCheck.end()){
 		if(it->is_valid()){
			if( it->status().state == libtorrent::torrent_status::finished || it->status().state == libtorrent::torrent_status::seeding ){
				sharedTorrentsWaitingCheck.erase(it);
			} else if( it->status().state == libtorrent::torrent_status::downloading || it->status().state == libtorrent::torrent_status::downloading_metadata){
				m_tmm.SetRemoved(it->info_hash());
				m_ts->remove_torrent(*it, libtorrent::session::delete_files); // Removing if some file was downloaded while bootstrapping.
				sharedTorrentsWaitingCheck.erase(it);
			}
 		}
	}
}
	
uint64 CTorrent::GetCompletedSize(CMD4Hash fileId){
	m_strategy->GetCompletedSize(fileId);
}

CTorrent::~CTorrent() {}

}

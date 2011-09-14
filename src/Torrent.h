/*
 * Torrent.h
 *
 *  Created on: Jun 10, 2011
 *      Author: Damian Vicino
 */

#ifndef TORRENT_H_
#define TORRENT_H_
#include <libtorrent/session.hpp>
#include <libtorrent/create_torrent.hpp>
#include <common/Path.h>
#include <boost/unordered_map.hpp>
#include <boost/filesystem.hpp>
#include "MD4Hash.h"
#include "TorrentMuleMapping.h"
#include "TorrentStrategy.h"
#include "DownloadQueue.h"
#include "UploadQueue.h"


namespace torrent {

/**
 * Wrap class for torrent functionalities.
 *
 * It is implemented as a singleton, to get the instance use: Torrent::getInstance() method.
 */
class CTorrent {
public:
	static const int NO_BT = 0; //! constant indetifying the NoBtStrategy.
	static const int ALLWAYS_FALL_TO_BT = 1; //! constant identifying the AlwaysFallToBTStrategy.
	static const int SWITCH_TO_THE_MOST_USABLE_PEERS = 2; //! constant identifying the SwitchToTheMostUsablePeersStrategy.

	/**
	 * Get the instance.
	 *
	 * @return the single CTorrent instance.
	 */
	static CTorrent& GetInstance();

	/**
	 * Starts the torrent session.
	 *
	 * Torrent sessions are a container for all the shared and downloading torrents and handle all the connections
	 * some ports are opened when starting the session and if prefered Mainline DHT is joined too.
	 */
	void StartTorrentSession();

	/**
	 * Closes all torrent conections and destroy session object.
	 */
	void EndTorrentSession();

	/**
	 * Port for the torrent incomming connections.
	 *
	 * This method is used to bootstrap Torrent traffic with known KAD peers.
	 */
	const int GetPort() const;

	/**
	 * Iterates the TorrentMuleMap to load/save metadata.
	 *
	 * Loads all the torrents info-files that are known but not loaded in session yet and saves all the
	 * torrent info-files of those that were received but not persisted yet.
	 */
	void RefreshMetadata();

	/**
	 * Creates torrent Metadata for a file.
	 *
	 * @param fileId The MD4 aMule identifier of the file.
	 * @param filename The name of the file.
	 * @param storeDir Path where the file is stored
	 */
	void CreateMetadataForFile(const CMD4Hash fileID, const CPath& filename, const CPath& storeDir);

	/**
	 * Creates torrent Metadata for a file.
	 *
	 * @param fileId The MD4 aMule identifier of the file.
	 * @param filename The name of the file.
	 * @param storeDir Path where the file is stored
	 */
	void CreateMetadataForFile(const CMD4Hash fileId, boost::filesystem::path filename, boost::filesystem::path storeDir);

	/**
	 * Checks if BT Metadata is known for a file identified with a MD4 aMule identifier.
	 *
	 * @param fileId A MD4 aMule identifier of a file.
	 */
	bool HasBTMetadata(const CMD4Hash fileId) const;

	/**
	 * Obtain the SHA1 content identifier for torrent knowing its muleId.
	 *
	 * @param fileId An MD4 identifier for a file known in aMule.
	 * @return SHA1 torrent content identifier.
	 */
	std::string GetBTIHAsString(const CMD4Hash fileId);
	
	/**
	 * Add a download to Torrent queue knowing SHA1 and a peer.
	 *
	 * If the file is not in the queue, it is added and the peer is associated hoping to get a full
	 * metadata exchange from it, else the peer is added to the set of known peers for that file.
	 *
	 * @param fileId An MD4 identifier for a file known in aMule.
 	 * @param SHA1Hash The SHA1 provided by KAD that identifies the file's torrent metadata.
	 */
	void AddDownloadUsingSHA1AndPeer(const CMD4Hash fileId, std::string SHA1Hash, std::string peerIP, int port);

	/**
	 * Add a download to Torrent queue having a .torrent file.
	 *
	 * The file is copied into the torrent files directory and an unregistered torrents call is made.
	 *
 	 * @param file The complete filename for a info-data torrent file.
	 * @return true if the file was registered successfully in tmm.
	 */
	bool AddDownloadUsingTorrentFile(boost::filesystem::path file);
	
	/**
	 * Add a download to Torrent queue having a torrent magnet link.
	 *
 	 * @param magnet A magnet link for torrent.
	 * @return true if the file was registered successfully in tmm.
	 */
	bool AddDownloadFromMagnet(std::string magnet);
		
	/**
	 * Process decides the way content should be downloaded.
	 *
	 * This method is suppossed to be called in interval ticks as same as CDownloadQueue::Process
	 * It reads Torrent and aMule queues and decides what to start, pause, stop in each of them based in
	 * the selected TorrentStrategy.
	 *
	 */
	void Process();

	/**
	 * Given a file get an estimation of how much of it was downloaded.
	 *
	 * @param fileId The MD4 hash identification of a file in the aMule Download Queue.
	 */
	uint64 GetCompletedSize(CMD4Hash fileId);

	/**
	 * Starts Mainline DHT service.
	 */
	void StartMainline();

	/**
	 * Stops Mainline DHT service.
	 */
	void StopMainline();

	/**
	 * Check if Mainline DHT service is runnning.
	 * 
	 * @return true if the Mainline DHt is running.
	 */
	bool IsMainlineConnected();

	/**
	 * Checks the ThePrefs::TorrentDir for info-files that are known yet.
	 *
	 * This method iterates the dir, and queue all unknown torrents into actual session for download.
	 * All Torrents in the Torrent metadata directory are loaded, it works as a Drop box for Torrents.
	 */
	void LoadUnregisteredTorrents();

	/**
	* Saves torrent metadata into file.
	*
	* @param t create_torrent instance of the file to be persisted.
	* @param filename The filename of the content.
	* @return filename of the saved info-file torrent metadata.
	*/
	boost::filesystem::path SaveTorrent(libtorrent::create_torrent & t, boost::filesystem::path & filename);

	/**
	 * If downloading in bt this file, just give up.
	 *
	 * @param fileId A MD4 aMule identifier of a file.
	 */

	void GiveUp(CMD4Hash);
	
	/**
	 * Destructor
	 */
	virtual ~CTorrent();
private :
	/**
	 * Avoid construction to be singleton
	 */
	CTorrent();

	/**
	 * Avoid construction to be singleton
	 */
	CTorrent(CTorrent const&);

	/**
	 * Avoid construction to be singleton
	 */
	void operator=(CTorrent const&);

	/**
	 * Sets the strategy to be use to handle Torrent/aMule collaboration.
	 *
	 * @param strategy check the constant values available at start of this class.
	 */
	void SetStrategy(int strategy);

	/**
	 * Check the files that were loaded as shared ones to see if any of them was removed.
	 */
	void CheckLoadedSharedTorrents();

	/**
	* Reads a torrent metadata info-file, adds it to the session and returns a torrent handle to it.
	*
	* @param filename The filename where the torrent metadata is persisted.
	* @return A torrent handle to the loaded torrent.
	*/
	libtorrent::torrent_handle& LoadMetadataFile(boost::filesystem::path& filename);

	CTorrentStrategy* m_strategy; //! Selected strategy for data transfer.
	libtorrent::session * m_ts; //! Active torrent session instance.
	CTorrentMuleMapping m_tmm; //! Active MetadataRelations.
	std::vector<libtorrent::torrent_handle> sharedTorrentsWaitingCheck;
};

}


#endif /* TORRENT_H_ */

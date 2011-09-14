/*
 * TorrentStrategy.h
 *
 *  Created on: Jul 26, 2011
 *      Author: Damian Vicino
 */

#ifndef TORRENTSTRATEGY_H_
#define TORRENTSTRATEGY_H_
#include <vector>
#include "MD4Hash.h"
#include <libtorrent/peer_id.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/alert_types.hpp>
#include "DownloadQueue.h"
#include "SharedFileList.h"
#include "TorrentMuleMapping.h"

namespace torrent {

const int MAX_ALLOWED_VALIDATION_TRIES=6;
	
/**
 * Abtract class for Transfer protocol selection strategies.
 *
 * Different strategies can be created to handle the way it is decided to choose transfer protocol
 * and when to switch to the other one.
 */
class CTorrentStrategy {
public:
	/**
	 * Constructor.
	 *
	 * @param torrentSession A pointer to the running torrent session.
	 * @param mapping A pointer to the MetadataRelations between Torrent and aMule.
	 */
	CTorrentStrategy(libtorrent::session* torrentSession, CTorrentMuleMapping* mapping);

	/**
	 * Process decides the way content should be downloaded.
	 *
	 * Abstract method that should read Torrent and aMule queues and decides what to start, pause,
	 * stop in each of them, also decides when alerts from asynchronous operations in torrent
	 * should be handled and how should they be handled.
	 *
	 */
	virtual void Process()=0;

	/**
	 * Given a file get an estimation of how much of it was downloaded.
	 *
	 * @param fileId The MD4 hash identification of a file in the aMule Download Queue.
	 */
	uint64 GetCompletedSize(CMD4Hash fileId);

	/**
	 * Removes any internal representation of torrents that are no longer needed.
	 * 
	 * @param fileId The MD4 hash identification of a file in the aMule Download Queue.
	 */
	virtual void GiveUp(CMD4Hash fileId); 
	
	/**
	 * Destructor
	 */
	virtual ~CTorrentStrategy();
protected:
	/**
	 * Processes the alerts coming from asynchronous torrent tasks.
	 */
	void ProcessAlert(std::auto_ptr<libtorrent::alert>);

	/**
	 * Process the received metadata alert 
	 * 
	 * If not overrided by inheritance, it will save the received metadata in a torrent file and exit.
	 */
	virtual void OnReceivedMetadata(libtorrent::metadata_received_alert*);

	/**
	 * Process the received torrent resume alert 
	 * 
	 * If not overrided by inheritance, it will just do nothing.
	 */
	virtual void OnTorrentResumed(libtorrent::torrent_resumed_alert*);
	
	/**
	 * Process the received finished download alert 
	 * 
	 * If not overrided by inheritance, it will just do nothing.
	 */
	virtual void OnFinishedDownload(libtorrent::torrent_finished_alert * );

	/**
	 * Check the torrents that were finished if any of them failed the MD4 check.
	 */
	void ValidateDownloads();
	
	
	uint32 m_lastTimeProcessWasRun; //! last time the Process method was called for this strategy.
	uint32 m_lastTimeAlertsWereProcessed; //! last time the Alerts for torrent asynchronous task were processed.
	uint32 m_lastTimeValidationQueueProcessed; //! last time the Validation Queue was processed.
	libtorrent::session * m_ts; //! pointer to the active torrent session.
	CTorrentMuleMapping * m_tmm; //! pointer to the Metadata Relation for torrent and amule.
	std::vector<std::pair<CMD4Hash, int> > m_validationQueue; //! This Vector keeps record of those hashes awaiting validation and how many tries for validation were done.

	/**
	 * Prevent default constructor
	 */
	CTorrentStrategy();
};

/**
 * A strategy that Falls to Torrent transfer protocol as soon as BTIH is known
 *
 * This strategy checks if any peer provided a Info Hash for Torrent transfer and
 * as soon as it is transfered pauses the download in the aMule download queue and
 * fully transfer the content using BitTorrent transfer protocol.
 * It is not a very smart strategy but is great for debugging.
 * Check CTorrentStrategy comments.
 */
class CTorrentAlwaysFallToBTStrategy: public CTorrentStrategy {
public:
	/**
	 * Constructor
	 * Same as CTorrentStrategy constructor so far
	 *
	 * @param torrentSession A pointer to the running torrent session.
	 * @param mapping A pointer to the MetadataRelations between Torrent and aMule.
	 */
	CTorrentAlwaysFallToBTStrategy(libtorrent::session* torrentSession, CTorrentMuleMapping* mapping);

	/**
	 * When new BT info has was received moves the download to Torrent Protocol and start ProcessAlerts
	 *
	 * @see CTorrentStrategy::Process and CTorrentStrategy::ProcessAlerts
	 */
	void Process();
	
	/**
	 * Destructor
	 */
	virtual ~CTorrentAlwaysFallToBTStrategy();
private:
	/**
	 * Process the received torrent resume alert 
	 * 
	 * Will pause any overlapping tranfer in the amulequeue
	 */
	void OnTorrentResumed(libtorrent::torrent_resumed_alert*);
	
	/**
	 * Prevent default constructor
	 */
	CTorrentAlwaysFallToBTStrategy();
};

/**
 * A strategy that doesn't use Torrent Protocol for transfers, just noop and back.
 *
 */
class CNoTorrentStrategy: public CTorrentStrategy {
public:
	/**
	 * Constructor
	 * Overrides CTorrentStrategy so nothing is done.
	 *
	 * @param torrentSession A pointer to the running torrent session.
	 * @param mapping A pointer to the MetadataRelations between Torrent and aMule.
	 */
	CNoTorrentStrategy(libtorrent::session* torrentSession, CTorrentMuleMapping* mapping);

	/**
	 * Does nothing.
	 *
	 * @see CTorrentStrategy::Process and CTorrentStrategy::ProcessAlerts
	 */
	void Process();
	

	/**
	 * Destructor
	 */
	virtual ~CNoTorrentStrategy();
private:
	/**
	 * Prevent default constructor
	 */
	CNoTorrentStrategy();
	
	/**
	 * Process the received metadata alert.
	 * 
	 * After metadata was downloaded, save it and set the mode to upload only.
	 */
	void OnReceivedMetadata(libtorrent::metadata_received_alert*);
};

/**
 * A strategy that switches always to the protocol with more known sources.
 *
 * The Strategy pauses the download in the protocol with less know sources and every
 * few iterations resumes the both so more sources can be found, after few seconds,
 * decides again for only one.
 *
 */
class CTorrentSwitchToTheMostUsablePeersStrategy: public CTorrentStrategy {
public:
	/**
	 * Constructor
	 * Overrides CTorrentStrategy so nothing is done.
	 *
	 * @param torrentSession A pointer to the running torrent session.
	 * @param mapping A pointer to the MetadataRelations between Torrent and aMule.
	 */
	CTorrentSwitchToTheMostUsablePeersStrategy(libtorrent::session* torrentSession, CTorrentMuleMapping* mapping);

	/**
	 * Does nothing.
	 *
	 * @see CTorrentStrategy::Process and CTorrentStrategy::ProcessAlerts
	 */
	void Process();

	/**
	 * Removes any internal representation of torrents that are no longer needed.
	 * 
	 * @param fileId The MD4 hash identification of a file in the aMule Download Queue.
	 */
	void GiveUp(CMD4Hash fileId); 
	
	/**
	 * Destructor
	 */
	virtual ~CTorrentSwitchToTheMostUsablePeersStrategy();
private:
	/**
	 * Prevent default constructor
	 */
	CTorrentSwitchToTheMostUsablePeersStrategy();
	
	/**
	 * Process the received finished download alert 
	 * 
	 * If not overrided by inheritance, it will just do nothing.
	 */
	void OnFinishedDownload(libtorrent::torrent_finished_alert * );

	
	uint32 m_lastTimeOptimisticResume; //! The time when the last Optimistic resume was done.
	uint32 m_lastTimeAppliedPauses; //! The time when the last pauses were applied.
	std::vector<CMD4Hash> m_pausedMule; //! MD4 Id of downloads that were paused in Mule Download Queue.
	std::vector<libtorrent::sha1_hash> m_pausedTorrent; //! SHA1 Id of downloads that were paused in torrent session.
};


} /* namespace torrent */
#endif /* TORRENTSTRATEGY_H_ */

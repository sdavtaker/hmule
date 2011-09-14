/*
 * TorrentMuleMapping.h
 *
 *  Created on: Jun 27, 2011
 *      Author: Damian Vicino
 */

#ifndef TORRENTMULEMAPPING_H_
#define TORRENTMULEMAPPING_H_
#include <boost/tuple/tuple.hpp>
#include <boost/unordered_map.hpp>
#include <boost/filesystem.hpp>
#include <string>
#include <libtorrent/peer_id.hpp>
#include "MD4Hash.h"

namespace torrent {
/**
 * Possible states of a content.
 *
 * It is to track of what was the client doing with each content between sessions
 * to avoid double download a content or some other weird situations.
 */
typedef enum{
	downloading, //! Means the file is in some download queue (aMule or torrent).
	sharing, //! The file was downloaded from somewhere or provided by user, just share it.
	removed //! The file was removed by the user, can't be shared, don't download it.
} state;

/**
 * Metadata Relation is a 4-tuple with relations between aMule and Torrent metadata.
 *
 * It contains a MD4 to identify the content in aMule, a SHA1 to identify the content as Torrent, a path
 * to the .torrent info-file containning the full Torrent metadata, and a state of the content.
 */
typedef boost::tuple<CMD4Hash*, libtorrent::sha1_hash*, boost::filesystem::path*, state> MetadataRelation;

/**
 * Hash function for MuleIdToMetadataRelation indexing
 */
struct MD4ToHash
    : std::unary_function<CMD4Hash, std::size_t>
{
    std::size_t operator()(CMD4Hash const& v) const
    {
        std::size_t seed = 0;
        std::string x = v.EncodeSTL();
        for(std::string::const_iterator it = x.begin();
            it != x.end(); ++it)
        {
            boost::hash_combine(seed, *it);
        }

        return seed;
    }
};

/**
 * Hash function for BTIdToMetadataRelation indexing
 */
struct SHA1ToHash
	: std::unary_function<libtorrent::sha1_hash, std::size_t>
{
	std::size_t operator()(libtorrent::sha1_hash const& v) const
	{
		std::size_t seed = 0;
		std::string x = v.to_string();
		for(std::string::const_iterator it = x.begin();
			it != x.end(); ++it)
		{
			boost::hash_combine(seed, *it);
		}

		return seed;
	}
};

/**
 * Hash function for InfoFileToMetadataRelation indexing
 */
struct filenameToHash
	: std::unary_function<boost::filesystem::path, std::size_t>
{
	std::size_t operator()(boost::filesystem::path const& v) const
	{
		std::size_t seed = 0;
		std::string x = v.native();
		for(std::string::const_iterator it = x.begin();
			it != x.end(); ++it)
		{
			boost::hash_combine(seed, *it);
		}

		return seed;
	}
};

/**
 * MuleIdToMetadataRelation is a Map used to provide indexing of the known MetadataRelations by Mule MD4 Identifiers.
 */
typedef boost::unordered_map<CMD4Hash, MetadataRelation*, MD4ToHash >  MuleIdToMetadataRelation;

/**
 * BTIdToMetadataRelation is a Map used to provide indexing of the known MetadataRelations by Torrent SHA1 Identifiers.
 */
typedef boost::unordered_map<libtorrent::sha1_hash, MetadataRelation*, SHA1ToHash >  BTIdToMetadataRelation;

/**
 * InfoFileToMetadataRelation is a Map used to provide indexing of the known MetadataRelations by Torrent's metadata filename.
 */
typedef boost::unordered_map<boost::filesystem::path, MetadataRelation* , filenameToHash>  InfoFileToMetadataRelation;

/**
 * CTorrentMuleMapping is a container for the MetadataRelations.
 */
class CTorrentMuleMapping {
public:
	/**
	 * Updates or creates a MetadataRelation.
	 *
	 * It looks for MetadataRelations partially containing the data provided and replaces the null data
	 * so the Metadata Relation increases knowledge, it can't remove info or modify it, only increment it.
	 * Usually this Update is used when a file starts sharing or when a download started in KAD and some peer
	 * provided the info-file metadata for Torrent transfer. (both cases content metadata is completely known).
	 *
	 * @param muleId A MD4 identifier of an aMule known file.
	 * @param torrentId A SHA1 identifier of a torrent known content.
	 * @param torrentFile A filename of where the info-file metadata is saved to load it again in next session.
	 * @return Pointer to the created or updated Metadata Relation.
	 */
	MetadataRelation* UpdateMetadata(CMD4Hash muleId, libtorrent::sha1_hash torrentId, boost::filesystem::path torrentFile);

	/**
	 * Creates a MetadataRelation.
	 *
	 * It creates a MetadataRelation with the aMule Identifier and the BT info-hash, but keeps the
	 * torrentFile as NULL.
	 * Usually this Update is used when a download started in KAD and some peer provided BTIH, but the
	 * info-file metadata for Torrent transfer was not acquired yet.
	 *
	 * @param muleId A MD4 identifier of an aMule known file.
	 * @param torrentId A SHA1 identifier of a torrent known content.
	 * @return Pointer to the created or updated Metadata Relation.
	 */
	MetadataRelation* UpdateMetadata(CMD4Hash muleId, libtorrent::sha1_hash torrentId);

	/**
	 * Erases a MetadataRelation.
	 * 
	 * @param muleId A MD4 identifier of an aMule known file.
	 */
	void Erase(CMD4Hash muleId);
	
	/**
	 * Creates a MetadataRelation.
	 *
	 * It creates a MetadataRelation with the BT info-hash and the filename of the info-file containing
	 * torrent metadata and keeps the torrentFile as NULL.
	 * Usually this Update is used when a download started in from mainline or .torrent file and the
	 * MD4 identifier for aMule is not known yet.
	 *
	 * @param torrentId A SHA1 identifier of a torrent known content.
	 * @param torrentFile A filename of where the info-file metadata is saved to load it again in next session.
	 * @return Pointer to the created or updated Metadata Relation.
	 */
	MetadataRelation* UpdateMetadata(libtorrent::sha1_hash torrentId, boost::filesystem::path torrentFile); // downloading from BT, no MD4 generated yet

	// There is no Unary Updates sin  there nothing to relate until 2 are know.
	// Also there is no muleId, torrentFile Updates since when torrentFile
	// is known, BTIH should be known too.

	/**
	 * Set state of a relation as Downloading.
	 *
	 * @param muleId A MD4 identifier for a file known in aMule.
	 */
	void SetDownloading(CMD4Hash muleId);

	/**
	 * Set state of a relation as Downloading.
	 *
	 * @param torrentId A SHA1 identifier for a content known in Torrent.
	 */
	void SetDownloading(libtorrent::sha1_hash torrentId);

	/**
	 * Set state of a relation as Sharing.
	 *
	 * @param muleId A MD4 identifier for a file known in aMule.
	 */
	void SetSharing(CMD4Hash muleId);

	/**
	 * Set state of a relation as Sharing.
	 *
	 * @param torrentId A SHA1 identifier for a content known in Torrent.
	 */
	void SetSharing(libtorrent::sha1_hash torrentId);

	/**
	 * Set state of a relation as Removed.
	 *
	 * @param muleId A MD4 identifier for a file known in aMule.
	 */
	void SetRemoved(CMD4Hash muleId);

	/**
	 * Set state of a relation as Removed.
	 *
	 * @param torrentId A SHA1 identifier for a content known in Torrent.
	 */
	void SetRemoved(libtorrent::sha1_hash torrentId);
	
	/**
	 * Check if the muleId has some related torrent metadata info-file.
	 *
	 * @param muleId A MD4 identifier for a file known in aMule.
	 * @return true if the muleId has some related torrent metadata info-file.
	 */
	bool HasTorrentPath(CMD4Hash muleId) const;

	/**
	 * Check if the torrentId has some related torrent metadata info-file.
	 *
	 * @param torrentId A SHA1 identifier for a content known in Torrent.
	 * @return true if the torrentId has some related torrent metadata info-file.
	 */
	bool HasTorrentPath(libtorrent::sha1_hash torrentId) const;

	/**
	 * Check if the torrentFile is declared
	 *
	 * @param torrentFile A filename of where the info-file metadata is saved.
	 * @return true if the torrentFile is declared.
	 */
	bool HasTorrentPath(boost::filesystem::path torrentFile) const;

	/**
	 * Check if the muleId has some related torrent SHA1 identifier.
	 *
	 * @param muleId A MD4 identifier for a file known in aMule.
	 * @return true if the muleId has some related torrent SHA1 identifier.
	 */
	bool HasBTIH(CMD4Hash muleId) const;

	/**
	 * Check if the BT info-hash is declared
	 *
	 * @param torrentId A SHA1 identifier for a content known in Torrent.
	 * @return true if the BT info-hash is declared.
	 */
	bool HasBTIH(libtorrent::sha1_hash torrentId) const;

	/**
	 * Check if the info-file has some related torrent SHA1 identifier.
	 *
	 * @param torrentFile A filename of where the info-file metadata is saved.
	 * @return true if the info-file has some related torrent SHA1 identifier.
	 */
	bool HasBTIH(boost::filesystem::path torrentFile) const;

	/**
	 * Check if the muleId is declared.
	 *
	 * @param muleId A MD4 identifier for a file known in aMule.
	 * @return true if the muleId is declared.
	 */
	bool HasMuleIH(CMD4Hash muleId) const;

	/**
	 * Check if the info-file has some related MD4 aMule identifier.
	 *
	 * @param torrentId A SHA1 identifier for a content known in Torrent.
	 * @return true if the info-file has some related MD4 aMule identifier.
	 */
	bool HasMuleIH(libtorrent::sha1_hash torrentId) const;

	/**
	 * Check if the info-file has some related MD4 aMule identifier.
	 *
	 * @param torrentFile A filename of where the info-file metadata is saved.
	 * @return true if the info-file has some related MD4 aMule identifier.
	 */
	bool HasMuleIH(boost::filesystem::path torrentFile) const;

	/**
	 * Obtain the filename of a torrent info-file knowing its muleId.
	 * 
	 * @warning This method assumes you asking for a valid tuple, check before use.
	 * @see HasTorrentPath
	 *
	 * @param muleId An MD4 identifier for a file known in aMule.
	 * @return filename of the torrent info-file.
	 */
	const boost::filesystem::path& GetTorrentPath(CMD4Hash muleId) const;

	/**
	 * Obtain the filename of a torrent info-file knowing its torrentId.
	 * 
	 * @warning This method assumes you asking for a valid tuple, check before use.
	 * @see HasTorrentPath
	 *
	 * @param torrentId An SHA1 identifier for a content known in Torrent.
	 * @return filename of the torrent info-file.
	 */
	const boost::filesystem::path& GetTorrentPath(libtorrent::sha1_hash torrentId) const;

	/**
	 * Obtain the SHA1 content identifier for torrent knowing its muleId.
	 * 
	 * @warning This method assumes you asking for a valid tuple, check before use.
	 * @see HasBTIH
	 *
	 * @param muleId An MD4 identifier for a file known in aMule.
	 * @return SHA1 torrent content identifier.
	 */
	const libtorrent::sha1_hash& GetBTIH(CMD4Hash muleId) const;

	/**
	 * Obtain the SHA1 content identifier for torrent knowing the filename of its info-file.
	 *
	 * @warning This method assumes you asking for a valid tuple, check before use.
	 * @see HasBTIH
	 *
	 * @param torrentFile The name of a info-file containing torrent metadata.
	 * @return SHA1 torrent content identifier.
	 */
	const libtorrent::sha1_hash& GetBTIH(boost::filesystem::path torrentFile) const;

	/**
	 * Obtain the MD4 aMule file identifier for torrent knowing the torrent SHA1 content identifier.
	 *
	 * @warning This method assumes you asking for a valid tuple, check before use.
	 * @see HasMuleIH
	 *
	 * @param torrentId An SHA1 identifier for a content known in Torrent.
	 * @return MD4 identifier for a file known in aMule.
	 */
	const CMD4Hash& GetMuleIH(libtorrent::sha1_hash torrentId) const;

	/**
	 * Obtain the MD4 aMule file identifier for torrent knowing the filename of its torrent info-file.
	 *
	 * @warning This method assumes you asking for a valid tuple, check before use.
	 * @see HasMuleIH
	 *
	 * @param torrentFile The name of a info-file containing torrent metadata.
	 * @return MD4 identifier for a file known in aMule.
	 */
	const CMD4Hash& GetMuleIH(boost::filesystem::path torrentFile) const;

	/**
	 * Check if a file is in Downloading state knowing its MD4 aMule identifier.
	 *
	 * @param muleId An MD4 identifier for a file known in aMule.
	 * @return true if the file is in state Downloading.
	 */
	bool IsDownloading(CMD4Hash muleId);

	/**
	 * Check if a content is in Downloading state knowing its SHA1 torrent identifier.
	 *
	 * @param torrentId An SHA1 identifier for a content known in Torrent.
	 * @return true if the file is in state Downloading.
	 */
	bool IsDownloading(libtorrent::sha1_hash torrentId);

	/**
	 * Check if a content is in Downloading state knowing its torrent filename.
	 *
	 * @param torrentFile The name of a info-file containing torrent metadata.
	 * @return true if the file is in state Downloading.
	 */
	bool IsDownloading(boost::filesystem::path&);

	/**
	 * Check if a file is in Sharing state knowing its MD4 aMule identifier.
	 *
	 * @param muleId An MD4 identifier for a file known in aMule.
	 * @return true if the file is in state Sharing.
	 */
	bool IsSharing(CMD4Hash muleId);

	/**
	 * Check if a content is in Sharing state knowing its SHA1 torrent identifier.
	 *
	 * @param torrentId An SHA1 identifier for a content known in Torrent.
	 * @return true if the file is in state Sharing.
	 */
	bool IsSharing(libtorrent::sha1_hash torrentId);

	/**
	 * Check if a content is in Sharing state knowing its torrent filename.
	 *
	 * @param torrentFile The name of a info-file containing torrent metadata.
	 * @return true if the file is in state Sharing.
	 */
	bool IsSharing(boost::filesystem::path&);

	/**
	 * Check if a content Was Removed knowing its torrent filename.
	 *
	 * @param torrentFile The name of a info-file containing torrent metadata.
	 * @return true if the file is in state removed.
	 */
	bool WasRemoved(boost::filesystem::path&);

	/**
	 * Iterator type, only const iteration is allowed.
	 */
	typedef std::vector<MetadataRelation*>::const_iterator const_iterator;

	/**
	 * Standard iterator begin
	 */
	const_iterator begin() const;

	/**
	 * Standard iterator end
	 */
	const_iterator end() const;

	/**
	 * It loads a persisted instance of the class from filename
	 */
	void Load(boost::filesystem::path filename);

	/**
	 * It persists actual instance into filename.
	 */
	void Save(boost::filesystem::path filename);

	/**
	 * Destructor
	 */
	virtual ~CTorrentMuleMapping();
private:
	/**
	 * A helper generic method that handles all the Updates to the Metadata Relations.
	 * All params are pointers that can be NULL if the data for that param is known.
	 *
	 * @param muleId Pointer to a MD4 identifier of an aMule known file.
	 * @param torrentId Pointer to a SHA1 identifier of a torrent known content.
	 * @param torrentFile Pointer to a filename of where the info-file metadata is saved to load it again in next session.
	 * @return Pointer to the created or updated Metadata Relation.
	 */
	MetadataRelation* UpdateMetadata(CMD4Hash* muleId, libtorrent::sha1_hash* BTId, boost::filesystem::path* torrentPath);

	MuleIdToMetadataRelation m_MuleIHDictionary; //! All MetadataRelations indexed by MD4 aMule file identifier.
	BTIdToMetadataRelation m_BTIHDictionary;  //! All MetadataRelations indexed by SHA1 torrent content identifier.
	InfoFileToMetadataRelation m_BTFileDictionary;  //! All MetadataRelations indexed by torrent info-file filename.

	std::vector<MetadataRelation*> m_IterableMetadataRelations;  //! A vector with all the known MetadataRelations.
};

}

#endif /* TORRENTMULEMAPPING_H_ */

//! \file song.h
#ifndef SONG_H
#define SONG_H
#include <stdint.h>
#include "core.h"

//! Song class.
/*!
 * The song consists of a track map and a tag map.
 *
 * The tracks represent channels or individual phrases and contain sequence data. Track IDs (keys) are uint16_t.
 *
 * The tags represent song metadata (for example title and author) as well as envelopes and other platform-specific data
 * that cannot be easily represented in a portable way. Tags consists of a string vector and tag map keys are also strings.
 * The # prefix is used for song metadata, @ for instruments or envelope data. % prefix is special and used for
 * platform-specific events.
 */
class Song
{
	private:
		Tag_Map tag_map;
		Track_Map track_map;
		uint16_t ppqn;

	public:
		Song();

		Tag_Map& get_tag_map();
		void add_tag(const std::string& key, std::string value);
		void add_tag_list(const std::string &key, const std::string &value);
		void set_tag(const std::string& key, std::string value);
		Tag& get_tag(const std::string& key);
		const std::string& get_tag_front(const std::string& key) const;

		Track& get_track(uint16_t id);
		Track& make_track(uint16_t id);

		Track_Map& get_track_map();

		uint16_t get_ppqn() const;
		void set_ppqn(uint16_t new_ppqn);
};
#endif


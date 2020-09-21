//! \file song.h
#ifndef SONG_H
#define SONG_H
#include <stdint.h>
#include <memory>
#include "core.h"

//! Song class.
/*!
 * The song consists of a track map and a tag map.
 *
 * The tracks represent channels or individual phrases and contain sequence data. Track IDs (keys) are uint16_t.
 *
 * The tags represent song metadata (for example title and author) as well as envelopes and other platform-specific data
 * that cannot be easily represented in a portable way. Tags consists of a string vector and tag map keys are also strings.
 * The # prefix is used for song metadata, @ for instruments or envelope data.
 *
 * The 'cmd_' prefix is special and used for platform-specific events. Use the register_platform_command() and
 * get_platform_command() to set and retrieve these tags.
 */
class Song
{
	public:
		Song();

		Tag_Map& get_tag_map();
		void add_tag(const std::string& key, std::string value);
		void add_tag_list(const std::string &key, const std::string &value);
		void set_tag(const std::string& key, std::string value);
		Tag& get_tag(const std::string& key);
		Tag& get_or_make_tag(const std::string& key);
		const std::string& get_tag_front(const std::string& key) const;

		int16_t register_platform_command(int16_t param, const std::string& value);
		Tag& get_platform_command(int16_t param);
		Tag& get_tag_order_list();

		Track& get_track(uint16_t id);
		Track& make_track(uint16_t id);

		Track_Map& get_track_map();

		uint16_t get_ppqn() const;
		void set_ppqn(uint16_t new_ppqn);

		std::shared_ptr<Driver> get_driver(unsigned int rate, VGM_Interface* vgm_interface) const;
		bool set_type(const std::string& key);
		const std::string& get_type() const;

	private:
		Tag_Map tag_map;
		Track_Map track_map;
		uint16_t ppqn;
		int16_t platform_command_index;
		std::string type;
};
#endif


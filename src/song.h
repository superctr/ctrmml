//! \file song.h
#ifndef SONG_H
#define SONG_H
#include <stdint.h>
#include "core.h"

class Song
{
	private:
		Tag_Map tag_map;
		Track_Map track_map;
	public:
		Song();
		~Song();

		Tag_Map& get_tag_map();
		void add_tag(const std::string& key, std::string value);
		void add_tag_list(const std::string &key, const std::string &value);
		void set_tag(const std::string& key, std::string value);
		Tag& get_tag(const std::string& key);
		std::string get_tag_front(const std::string& key);
		Track& get_track(uint16_t id);
		Track_Map& get_track_map();
};
#endif


#ifndef SONG_H
#define SONG_H
#include <stdint.h>
#include <list>
#include <map>
#include "track.h"

typedef std::vector<std::string> Tag;
typedef std::map<std::string,Tag> Tag_Map;
typedef std::map<uint16_t, class Track> Track_Map;

class Song
{
	protected:
		Tag_Map tag_map;
		Track_Map track_map;
	public:
		Song();
		~Song();

		Tag_Map *get_tags();
		void add_tag(std::string key, std::string value);
		void add_tag_list(std::string key, std::string value);
		void set_tag(std::string key, std::string value);
		Tag *get_tag(std::string key);
		std::string get_tag_front(std::string key);
		Tag_Map *get_tag_map();
		class Track *get_track(uint16_t id);
		Track_Map *get_track_map();
};
#endif


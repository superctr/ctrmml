#include <stdio.h>
#include <ctype.h>
#include "song.h"

Song::Song()
	: tag_map(),
	track_map()
{
}

Song::~Song()
{
}

Tag_Map *Song::get_tags()
{
	return &tag_map;
}

void Song::add_tag(std::string key, std::string value)
{
	// delete trailing spaces
	while(!value.empty() && isspace(value.back()))
		value.pop_back();
	tag_map[key] = Tag(1,value);
}

Tag *Song::get_tag(std::string key)
{
	return &tag_map.at(key);
}

std::string Song::get_tag_front(std::string key)
{
	return tag_map.at(key).front();
}

Tag_Map *Song::get_tag_map()
{
	return &tag_map;
}

class Track *Song::get_track(uint16_t id)
{
	return &track_map.at(id);
}

Track_Map *Song::get_track_map()
{
}

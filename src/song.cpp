#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "song.h"
#include "track.h"
#include "stringf.h"

//! Constructs a Song.
Song::Song()
	: tag_map(),
	track_map(),
	ppqn(24),
	platform_command_index(-32768)
{
}

//! Get a reference to the tag map.
Tag_Map& Song::get_tag_map()
{
	return tag_map;
}

//! Gets the tag with the specified key.
/*!
 * \exception std::out_of_range if not found
 */
Tag& Song::get_tag(const std::string& key)
{
	return tag_map.at(key);
}

//! Gets the tag with the specified key, otherwise creates a new tag.
/*!
 * If a new tag is created, an entry is added to the 'tag_order' tag.
 */
Tag& Song::get_or_make_tag(const std::string& key)
{
	auto lookup = tag_map.find(key);
	if(lookup != tag_map.end())
	{
		return lookup->second;
	}
	else
	{
		tag_map["tag_order"].push_back(key);
		return tag_map[key];
	}
}

//! Gets the tag order list.
Tag& Song::get_tag_order_list()
{
	return tag_map["tag_order"];
}

//! Gets the first value of the tag with the specified key.
/*!
 * \exception std::out_of_range if not found
 */
const std::string& Song::get_tag_front(const std::string& key) const
{
	return tag_map.at(key).front();
}

//! Appends a value to the tag with the specified key.
/*!
 * If key is not found, a new tag is created
 */
void Song::add_tag(const std::string& key, std::string value)
{
	// delete trailing spaces
	while(!value.empty() && isspace(value.back()))
		value.pop_back();
	get_or_make_tag(key).push_back(value);
}

//! Set the value to the tag with the specified key.
//! This function is also used to set the song settings (title, platform, ppqn, etc)
/*!
 * If key is not found, a new tag is created
 */
void Song::set_tag(const std::string& key, std::string value)
{
	// delete trailing spaces
	while(!value.empty() && isspace(value.back()))
		value.pop_back();
	Tag& tag = get_or_make_tag(key);
	tag.clear();
	tag.push_back(value);
}

//! add double-quote-enclosed string
/*!
 * If key is not found, a new tag is created
 */
static char* add_tag_enclosed(Tag *tag, char* s)
{
	// process string first
	char *head = s, *tail = s;
	while(*head)
	{
		if(*head == '\\' && *++head)
		{
			if(*head == 'n')
				*head = '\n';
			else if(*head == 't')
				*head = '\t';
		}
		else if(*head == '"')
		{
			head++;
			break;
		}
		*tail++ = *head++;
	}
	*tail++ = 0;
	tag->push_back(s);
	return head;
}

//! Append multiple values (separated by commas or spaces) to the tag with the specified key.
/*!
 * The list can be separated by commas OR spaces, but comma always forces a value to be added.
 *
 * If key is not found, a new tag is created.
 */
void Song::add_tag_list(const std::string& key, const std::string& value)
{
	Tag *tag = &get_or_make_tag(key);
	char *str = strdup(value.c_str());
	char *s = str;
	int last_char = 0;
	while(*s)
	{
		char* nexts = strpbrk(s, " \t\r\n\",;");
		char c;
		if(nexts == NULL)
		{
			// full string
			tag->push_back(s);
			break;
		}
		c = *nexts;
		if(c == '\"')
		{
			// enclosed string
			nexts = add_tag_enclosed(tag, s+1);
			last_char = c;
		}
		else
		{
			// separator
			*nexts++ = 0;
			if(strlen(s))
			{
				tag->push_back(s);
				last_char = c;
			}
			else if(c == ',') // empty , block adds an empty tag
			{
				if(last_char == c)
					tag->push_back(s);
				else
					last_char = c;
			}
			if(c == ';')
				break;
		}
		s = nexts;
	}
	free(str);
}

//! Registers a platform command.
/*!
 * The command can later be retrieved with get_platform_command().
 *
 * \param param Set to -1 to use a sequential index.
 * \return The tag's param ID.
 */
int16_t Song::register_platform_command(int16_t param, const std::string& value)
{
	if(param == -1)
		param = platform_command_index++;
	add_tag_list(stringf("cmd_%d", param), value);
	return param;
}

//! Gets the registered platform command with the specified key.
/*!
 * \exception std::out_of_range if not found
 */
Tag& Song::get_platform_command(int16_t param)
{
	return tag_map.at(stringf("cmd_%d", param));
}

//! Get reference to the track map.
Track_Map& Song::get_track_map()
{
	return track_map;
}

//! Get a reference to the track with the specified id.
/*!
 * \exception std::out_of_range if not found. Use make_track() to create track if needed.
 */
Track& Song::get_track(uint16_t id)
{
	return track_map.at(id);
}

//! Get a reference to the track with the specified id.
//! If the track is not found, a new one is created.
Track& Song::make_track(uint16_t id)
{
	if(!track_map.count(id))
		return track_map.emplace(id, Track(ppqn)).first->second;
	else
		return track_map.at(id);
}

//! Gets the global Pulses per quarter note (PPQN) setting.
uint16_t Song::get_ppqn() const
{
	return ppqn;
}

//! Sets the global Pulses per quarter note (PPQN) setting.
void Song::set_ppqn(uint16_t new_ppqn)
{
	ppqn = new_ppqn;
}


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdexcept>
#include "song.h"
#include "vgm.h"
#include "driver.h"
#include "track.h"
#include "stringf.h"
#include "platform/mdsdrv.h"

//! Constructs a Song.
Song::Song()
	: tag_map()
	, track_map()
	, ppqn(24)
	, platform_command_index(-32768)
{
	platform = new MDSDRV_Platform(0);
}

//! Destructs a Song
Song::~Song()
{
	if(platform)
		delete platform;
}

//! Get a reference to the tag map.
Tag_Map& Song::get_tag_map()
{
	return tag_map;
}

//! Check if the tag with the specified key is present
/*!
 * \exception std::out_of_range if not found
 */
bool Song::check_tag(const std::string& key) const
{
	return tag_map.find(key) != tag_map.end();
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

//! Gets the first value of the tag with the specified key. Should not throw exceptions.
/*!
 *  If the tag is not present or empty, an empty string ("") is returned instead.
 */
std::string Song::get_tag_front_safe(const std::string& key) const
{
	if(tag_map.count(key) && tag_map.at(key).size())
		return tag_map.at(key).front();
	else
		return "";
}

//! Appends a value to the tag with the specified key.
/*!
 *  Appends the value as a new item to the tag.
 *
 *  \param key Tag key. If the key is not found, a new tag is created.
 *  \param value String that is added to the tag.
 *  \see set_tag() add_tag_list()
 */
void Song::add_tag(const std::string& key, std::string value)
{
	// delete trailing spaces
	while(!value.empty() && isspace(value.back()))
		value.pop_back();
	get_or_make_tag(key).push_back(value);
}

//! Set the value to the tag with the specified key.
/*!
 *  This function is used to write song metadata, such as title,
 *  platform, ppqn, etc.
 *
 *  Overwrites all previous values that have been written to a tag
 *  and creates an item containing the whole string.
 *
 *  \param key Tag key. If the key is not found, a new tag is created.
 *  \param value String that is written to the tag.
 *  \see add_tag() add_tag_list()
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

//! Add double-quote-enclosed string
/*!
 *  \param key Tag key. If the key is not found, a new tag is created.
 *  \param s First character of the key, not including the first '"'.
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

//! Append multiple values to the tag with the specified key.
/*!
 *  \p value contains a string containing an array of values separated
 *  by spaces or commas. Each value is added as an item in the tag.
 *
 *  A value can be enclosed with quotes to support spaces.
 *
 *  A comma with no preceeding non-space character inserts a blank item.
 *
 *  \param key Tag key. If the key is not found, a new tag is created.
 *  \param value List of values to add. The list can be separated by
 *               commas OR spaces, but comma always forces a value to
 *               be added.
 *
 *  \see add_tag() set_tag()
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
 * \param param Command id. Set to -1 to use a sequential id.
 * \param value Command arguments.
 *              Parsed by Player::parse_platfrom_event()
 * \return The tag's command id.
 *
 * \see Player::parse_platform_event()
 */
int16_t Song::register_platform_command(int16_t param, const std::string& value)
{
	if(param == -1)
		param = platform_command_index++;
	add_tag_list(stringf("cmd_%d", param), value);
	return param;
}

//! Gets the registered platform command with the specified id.
/*!
 * \param param Command id.
 * \return Reference to the tag with the specified id.
 * \exception std::out_of_range if not found
 */
Tag& Song::get_platform_command(int16_t param)
{
	return tag_map.at(stringf("cmd_%d", param));
}

//! Get a reference to the track map.
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
/*!
 *  If the track is not found, a new one is created.
 */
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

//! Gets the platform
const Platform* Song::get_platform() const
{
	return platform;
}

//! Sets the platform
/*!
 *  \return 1 on failure
 *  \return 0 on success
 */
bool Song::set_platform(const std::string& key)
{
	if(iequal(key, "megadrive"))
	{
		delete platform;
		platform = new MDSDRV_Platform(0);
	}
	else if(iequal(key, "mdsdrv"))
	{
		delete platform;
		platform = new MDSDRV_Platform(2);
	}
	//type = key;
	return 0;
}

std::shared_ptr<Driver> Platform::get_driver(unsigned int rate, VGM_Interface* vgm_interface) const
{
	throw std::logic_error("No available driver");
}

const Platform::Format_List& Platform::get_export_formats() const
{
	static const Platform::Format_List out = {{"vgm", "VGM"}};
	return out;
}

std::vector<uint8_t> Platform::get_export_data(Song& song, int format) const
{
	if(format == 0)
	{
		return vgm_export(song);
	}
	else
	{
		throw std::logic_error("no such exporter");
	}
}

static inline std::string safe_get_tag(Song& song, const std::string& tagname)
{
	if(song.get_tag_map()[tagname].size())
		return song.get_tag_map()[tagname].front();
	else
		return "";
}

static inline VGM_Tag get_tags(Song& song)
{
	VGM_Tag tag;
	Tag_Map tag_map = song.get_tag_map();

	tag.title = safe_get_tag(song,"#title");
	tag.title_j = safe_get_tag(song,"#titlej");
	tag.author = safe_get_tag(song,"#composer");
	tag.author_j = safe_get_tag(song,"#composerj");
	tag.system = safe_get_tag(song,"#system");
	tag.system_j = safe_get_tag(song,"#systemj");
	tag.game = safe_get_tag(song,"#game");
	tag.game_j = safe_get_tag(song,"#gamej");
	tag.creator = safe_get_tag(song,"#programmer");
	tag.notes = safe_get_tag(song,"#comment");
	tag.date = safe_get_tag(song,"#vgmdate");

	// Fallback tags
	if(!tag.author.size())
		tag.creator = safe_get_tag(song,"#author");
	if(!tag.creator.size())
		tag.creator = safe_get_tag(song,"#programer");
	if(!tag.creator.size())
		tag.creator = tag.author;

	return tag;
}

std::vector<uint8_t> Platform::vgm_export(Song& song, unsigned int max_seconds, unsigned int num_loops) const
{
	VGM_Writer vgm("", 0x61, 0x100);
	auto driver = song.get_platform()->get_driver(44100, &vgm);
	unsigned long max_time = max_seconds * 44100;
	driver->play_song(song);
	double elapsed_time = 0;
	double delta = 0;
	bool looped_or_finished = 0;
	while(elapsed_time < max_time)
	{
		vgm.delay(delta);
		delta = driver->play_step();
		elapsed_time += delta;
		if(!driver->is_playing() || driver->get_loop_count() >= (int)num_loops)
		{
			looped_or_finished = 1;
			break;
		}
	}
	if(!looped_or_finished)
		vgm.delay(max_time-elapsed_time);
	vgm.stop();
	vgm.write_tag(get_tags(song));
	return vgm.get_buffer();
}

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "song.h"
#include "track.h"

Song::Song()
	: tag_map(),
	track_map()
{
}

Song::~Song()
{
}

Tag_Map& Song::get_tags()
{
	return tag_map;
}

void Song::add_tag(std::string key, std::string value)
{
	// delete trailing spaces
	while(!value.empty() && isspace(value.back()))
		value.pop_back();
	tag_map[key].push_back(value);
}

// add double-quote-enclosed string
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

// add a tag list
// list can be separated by commas OR spaces, but comma always forces a tag to be added
void Song::add_tag_list(std::string key, std::string value)
{
	Tag *tag = &tag_map[key];
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

Tag& Song::get_tag(std::string key)
{
	return tag_map.at(key);
}

std::string Song::get_tag_front(std::string key)
{
	return tag_map.at(key).front();
}

Tag_Map& Song::get_tag_map()
{
	return tag_map;
}

Track& Song::get_track(uint16_t id)
{
	return track_map.at(id);
}

Track_Map& Song::get_track_map()
{
	return track_map;
}


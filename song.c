#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ctrmml.h"

// return pointer to new tag
struct tag* tag_create(char* key, struct tag* property)
{
	struct tag *p = malloc(sizeof(*p));
	if(!p)
		return NULL;
	p->key = strdup(key);
	p->property = property;
	p->next = NULL;
	return p;
}

// delete tags and all children and next tags.
void tag_delete_recursively(struct tag* tag)
{
	while(tag)
	{
		struct tag* next = tag->next;
		if(tag->property)
			tag_delete_recursively(tag->property);
		free(tag->key);
		free(tag);
		tag = next;
	}
}

// get the next tag in the linked list
struct tag* tag_get_next(struct tag* tag)
{
	if(!tag)
		return NULL;
	return tag->next;
}

// append a tag to the end of the linked list
struct tag* tag_append(struct tag* tag, struct tag* new)
{
	if(tag == NULL)
		return NULL;
	while(tag->next)
		tag = tag->next;
	return tag->next = new;
}

// get child of a tag (null if none)
struct tag* tag_get_property(struct tag* tag)
{
	if(!tag)
		return NULL;
	return tag->property;
}

// set a tag as a 'child'. Deletes previous property
struct tag* tag_set_property(struct tag* tag, struct tag* new)
{
	if(tag == NULL)
		return NULL;
	if(tag->property)
		tag_delete_recursively(tag->property);
	return tag->property = new;
}

// add a tag to the list of properties
struct tag* tag_add_property(struct tag* tag, struct tag* new)
{
	if(tag == NULL)
		return NULL;
	if(tag->property)
		return tag_append(tag->property, new);
	else
		return tag->property = new;
}

// find tag with a specified key
struct tag* tag_find(struct tag* tag, char* key)
{
	while(tag)
	{
		if(!strcmp(tag->key, key))
			return tag;
		tag = tag->next;
	}
	return NULL;
}

// add tag
void add_tag(struct tag* tag, char* s)
{
	// trim space at end of the string
	while(strlen(s) > 0)
	{
		if(isspace(s[strlen(s)-1]))
			s[strlen(s)-1] = 0;
		else
			break;
	}
	tag_add_property(tag, tag_create(s, NULL));
	return;
}

// add double-quote-enclosed string
static char* add_tag_enclosed(struct tag* tag, char* s)
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
	tag_add_property(tag, tag_create(s, NULL));
	return head;
}

// add a tag list
// list can be separated by commas OR spaces, but comma always forces a tag to be added
void add_tag_list(struct tag* tag, char* s)
{
	int last_char = 0;
	while(*s)
	{
		char* nexts = strpbrk(s, " \t\r\n\",;");
		char c = *nexts;
		if(nexts == NULL)
		{
			// full string
			add_tag(tag, s);
			return;
		}
		else if(c == '\"')
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
				add_tag(tag, s);
				last_char = c;
			}
			else if(c == ',') // empty , block adds an empty tag
			{
				if(last_char == c)
					add_tag(tag, s);
				else
					last_char = c;
			}
			if(c == ';')
				return;
		}
		s = nexts;
	}
}

void song_free(struct song* song)
{
	int i;
	for(i=0; i<256; i++)
		track_free(song->track[i]);
	tag_delete_recursively(song->tag);
}

struct song* song_create()
{
	int i;
	struct song* song = calloc(1, sizeof(*song));
	if(!song)
		exit(-1);
	for(i=0; i<256; i++)
		song->track[i] = track_init();
	song->tag = tag_create("#format",tag_create("ctrmml", NULL));
	return song;
}

// dumps tags
static void dump_tags(struct tag* tag, int child)
{
	if(tag == NULL)
		return;
	while(tag)
	{
		struct tag* property = tag_get_property(tag);
		if(child)
			printf(" ");
		if(strpbrk(tag->key, " \"\\") || !strlen(tag->key))
		{
			char* c = tag->key;
			printf("\"");
			while(*c)
			{
				if(*c == '\"' || *c == '\\')
					printf("\\");
				printf("%c", *c++);
			}
			printf("\"");
		}
		else
			printf("%s", tag->key);
		if(!child && property)
		{
			dump_tags(tag_get_property(tag), 1);
		}
		tag = tag_get_next(tag);
		printf("%s", (!child) ? "\n" : "");
	}
}

int song_finalize(struct song* song)
{
	int i;
	for(i=0; i<256; i++)
		track_finalize(song, song->track[i], i);
	dump_tags(song->tag, 0);
	return 0;
}

int song_open(struct song* song, char* filename)
{
	return song_convert_mml(song,filename);
}

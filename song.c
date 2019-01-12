#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ctrmml.h"

// return pointer to new tag
struct tag* tag_create(char* value, char* key)
{
	struct tag *p = malloc(sizeof(*p));
	if(!p)
		return NULL;
	p->key = strdup(key);
	p->property = NULL;
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
struct tag* tag_add_property(struct tag* tag, struct tag* new)
{
	if(tag == NULL)
		return NULL;
	if(tag->property)
		tag_delete_recursively(tag->property);
	return tag->property = new;
}

// add a tag to the list of properties
struct tag* tag_set_property(struct tag* tag, struct tag* new)
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

void song_free(struct song* song)
{
	int i;
	for(i=0; i<256; i++)
		track_free(song->track[i]);
}

struct song* song_create()
{
	int i;
	struct song* song = calloc(1, sizeof(*song));
	if(!song)
		exit(-1);
	for(i=0; i<256; i++)
		song->track[i] = track_init();
	return song;
}

int song_finalize(struct song* song)
{
	int i;
	for(i=0; i<256; i++)
		track_finalize(song->track[i]);
}

int song_open(struct song* song, char* filename)
{
	return song_convert_mml(song,filename);
}

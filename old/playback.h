#ifndef PLAYBACK_H
#define PLAYBACK_H

// can return non zero to break loop
typedef int (*playback_cb)(void* pb_state);

struct pb_handler {
	double rate; // = x/44100 hz
	double delta_time;
	playback_cb callback;
	void* pb_state;
};

int playback(struct pb_handler *pb_handler, int pb_count, int samples);

#endif

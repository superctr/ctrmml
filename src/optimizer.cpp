// Optimizer code
//
// Basic flowchart:
//   1. Analyze stack depth.
//   2. Find matches.
//      a) Check if a loop match is found (same track ID, loop pos-start pos smaller than match length. If a loop match is found, assign score based on length
//      b) Check for subroutine matches. Assign score based on length AND number of matches found.
//   3. Pick the strategy with the highest score (out of one loop match and potentially multiple subroutine matches)
//   4. Perform the optimization and go back to step 1. If there are no good matches left, exit.
//
// TODO:
//   Potential performance improvements:
//     Switch to KMP search and/or hash tables.
//     If still too slow, switch to Multithreaded searches.
//   Look for more edge cases that could break loop optimizations...
//   Customizable minimum score.
//   Adjustable stack limits.

#include <cstdio>
#include "optimizer.h"
#include "song.h"
#include "input.h" // TrackRef
#include "player.h" // Song_Validator


// For debugging, it's useful to run each pass through the song validator, to check for changes in the
// song length and other looping errors. Only works when building mmlc
#if 1
#define VALIDATE_PASS
//void validate_song(Song& song);
#endif

const int Optimizer::max_stack_depth = 6;

const int Optimizer::min_sub_score = 3;
const int Optimizer::min_loop_score = 3;

const int Optimizer::max_sub_stack = 7;
const int Optimizer::max_loop_stack = 6;

int Optimizer::Stack_Analyzer::analyze_track(Song& song, Track& track, Optimizer& optimizer, int drum_mode)
{
	int loop_depth = 0;
	std::vector<Event>& events = track.get_events();
	event_list.clear();
	parsing = true;

	for(auto && event : events)
	{
		int jump_depth = 0;
		if(event.type == Event::LOOP_START)
			loop_depth++;
		if(event.type == Event::JUMP)
			jump_depth++;
		if(drum_mode && event.type == Event::NOTE)
			jump_depth++;

		int usage = jump_depth + loop_depth * 2;

		if(event.type == Event::JUMP)
		{
			Stack_Analyzer& dest = optimizer.stack_analyzer[event.param];
			if(dest.base_usage < (usage + base_usage))
			{
				dest.base_usage = usage + base_usage;
				if(!dest.parsing)
					drum_mode = dest.analyze_track(song, song.get_track(event.param), optimizer, drum_mode);
			}
			usage += dest.max_usage;
		}
		else if(event.type == Event::NOTE && drum_mode)
		{
			Stack_Analyzer& dest = optimizer.stack_analyzer[event.param];
			if(dest.base_usage < (usage + base_usage))
			{
				//printf("calling %d with usage %d+%d\n", drum_mode+event.param, usage, base_usage);
				dest.base_usage = usage + base_usage;
				if(!dest.parsing)
					dest.analyze_track(song, song.get_track(event.param), optimizer, 0);
			}
			usage += dest.max_usage;
		}
		else if(event.type == Event::DRUM_MODE)
		{
			drum_mode = event.param;
		}
		else if(event.type == Event::LOOP_END)
		{
			loop_depth--;
		}

		event_list.push_back(usage);
		if(usage > max_usage)
			max_usage = usage;
	}
	parsing = false;
	return drum_mode;
}

Optimizer::Optimizer(Song& song, int verbose)
	: sub_id(15000)
	, verbose(verbose)
	, min_score(10)
	, last_score(-1)
	, top_score(min_score)
	, song(&song)
	, best_match()
{
	auto& track_map = song.get_track_map();

	auto it = track_map.rbegin();
	if(it != track_map.rend() && it->first > sub_id)
	{
		sub_id = it->first + 1;
	}
}

void Optimizer::optimize()
{
	pass = 0;
	do
	{
		pass++;

		analyze_stack();
		find_best_match();
		if(verbose > 1)
			printf("Pass %d done, best score = %d\n", pass++, best_match.best_score());

		last_score = best_match.best_score();

		auto validator = Song_Validator(*song);
		if(verbose > 1)
		{
			unsigned int total_commands = 0;
			for(auto it = validator.get_track_map().begin(); it != validator.get_track_map().end(); it++)
			{
				unsigned int command_count = song->get_track(it->first).get_event_count();
				printf("Track%5d:%7d:%7d", it->first, command_count, it->second.get_play_time());
				if(auto length = it->second.get_loop_length())
					printf(" (loop %7d)", length);

				printf("\n");
				total_commands += song->get_track(it->first).get_event_count();
			}
			printf("Total # of commands:%7d\n", total_commands);
		}
	}
	while(best_match.best_score() > min_score);
}

void Optimizer::analyze_stack()
{
	stack_analyzer.clear();
	for(auto && track_it : song->get_track_map())
	{
		Stack_Analyzer& dest = stack_analyzer[track_it.first];
		if(!dest.base_usage)
		{
			dest.analyze_track(*song, track_it.second, *this, 0);
			// Don't optimized unused tracks or macro tracks. TODO: Platform specific
			if(track_it.first > 15)
				dest.base_usage = 100;
		}
	}
#if 0
	if(verbose > 1)
	{
		for(auto && it : stack_analyzer)
		{
			Stack_Analyzer& dest = stack_analyzer[it.first];
			if(dest.base_usage)
				printf("Track %d base usage = %d, stack usage = %d\n", it.first, it.second.base_usage, it.second.max_usage);
			else
				printf("Track %d stack usage = %d\n", it.first, dest.max_usage);
		}
	}
#endif
}

unsigned int Optimizer::find_match_length(uint32_t src_track, uint32_t src_start, uint32_t dst_track, uint32_t dst_start, unsigned int* loop_length)
{
	int loop_depth = 0;

	int src_end = src_start;
	int dst_end = dst_start;

	int dst_safe = dst_start;

	Track& src = song->get_track(src_track);
	Track& dst = song->get_track(dst_track);

	int src_count = src.get_event_count();
	int dst_count = dst.get_event_count();

	while(1)
	{
		if(src_end >= src_count || dst_end >= dst_count)
			break;
		//printf("dst %d pos %d\n", dst_end-dst_start, src_end);

		auto& src_event = src.get_event(src_end);
		auto& dst_event = dst.get_event(dst_end);

		int stack_depth = stack_analyzer[dst_track].event_list[dst_end] + stack_analyzer[dst_track].base_usage;

		if(stack_depth >= max_loop_stack)
			loop_length = nullptr;
		if(stack_depth >= max_sub_stack)
			break;
		else if(dst_event.type == Event::SEGNO || dst_event.type == Event::DRUM_MODE)
			break;
		else if((dst_event.type == Event::LOOP_END || dst_event.type == Event::LOOP_BREAK) && !loop_depth)
			break;

		if(dst_event.type == Event::LOOP_START)
			loop_depth++;
		else if(dst_event.type == Event::LOOP_END)
			loop_depth--;

		//printf("%d==%d,%d\n",src_event.param,dst_event.param,loop_depth);
		if((src_event.type == dst_event.type &&
			src_event.param == dst_event.param &&
			src_event.on_time == dst_event.on_time &&
			src_event.off_time == dst_event.off_time) ||
		   (src_event.type == dst_event.type && src_event.type == Event::LOOP_BREAK)) // Player modifies the param though it's normally unused
		{
			src_end++;
			dst_end++;

			if(!loop_depth)
			{
				dst_safe = dst_end;
				if(loop_length)
					(*loop_length)++;
			}
		}
		else
		{
			break;
		}
	}
	//printf("pos=[%d,%d],[%d,%d], loop_depth = %d, safe=%d, end=%d\n",src_track,src_start,dst_track,dst_start,loop_depth,dst_safe,dst_end);
	return dst_safe - dst_start;
}

Optimizer::Match Optimizer::find_match(uint32_t src_track, uint32_t src_start)
{
	Optimizer::Match match = {};
	std::map<uint32_t,uint32_t> subroutine_count;
	std::map<uint32_t,uint32_t> last_match;

	auto& track_map = song->get_track_map();

	for(auto && dst : track_map)
	{
		last_match.clear();
		if(dst.first < src_track)
			continue;
		else if(dst.first == src_track)
		{
			int loop_depth = 0;
			bool loop_valid = true;
			for(unsigned int dst_pos = src_start + 1; dst_pos < dst.second.get_event_count(); dst_pos++)
			{
				// Keep track of the loop depth, as we cannot break the loop hierarchy when creating a new loop
				auto param = dst.second.get_event(dst_pos).type;
				if(param == Event::SEGNO)
					loop_valid = false;
				else if((param == Event::LOOP_END || param == Event::LOOP_BREAK) && !loop_depth)
					loop_valid = false; //can't do loop matches beyond this point
				else if(param == Event::LOOP_END)
					loop_depth--;
				else if(param == Event::LOOP_START)
					loop_depth++;

				unsigned int loop_length = 0;
				unsigned int length = find_match_length(src_track, src_start, dst.first, dst_pos, &loop_length);
				if(!length)
					continue;

				// is it a loop?
				if(loop_valid && !loop_depth && loop_length >= min_loop_score && loop_length > match.loop_length)
				{
					match.loop_length = loop_length;
					match.loop_position = dst_pos;
				}
				// subroutine cannot be longer than the distance between the start and itself
				if(length > (dst_pos - src_start))
					length = dst_pos - src_start;
				// Matches of the same length cannot overlap so we check the distance from the
				// previous match with the same length
				while(length > min_sub_score)
				{
					if(dst_pos - last_match[length] >= length)
					{
						last_match[length] = dst_pos;
						subroutine_count[length]++;
					}
					length--;
				}
			}
		}
		else if(dst.first > src_track)
		{
			for(unsigned int dst_pos = 0; dst_pos < dst.second.get_event_count(); dst_pos++)
			{
				//printf("comparing with track %d pos %d\n", dst.first, dst_pos);
				unsigned int length = find_match_length(src_track, src_start, dst.first, dst_pos);
				while(length >= min_sub_score)
				{
					// special case here since we don't have to worry about overlap for the first
					// match and can't initialize the default element of the map
					if(!last_match[length] || dst_pos - last_match[length] >= (length + 1))
					{
						last_match[length] = dst_pos + 1;
						subroutine_count[length]++;
					}
					length--;
				}
			}
		}
	}
	match.track_id = src_track;
	match.position = src_start;

	for(auto && i : subroutine_count)
	{
		int32_t score = ((i.first - 1) * i.second) - 1;
		if(score > match.sub_score)
		{
			match.sub_length = i.first;
			match.sub_score = score;
			match.sub_repeats = i.second;
		}
	}

	return match;
}

void Optimizer::find_best_match()
{
	auto& track_map = song->get_track_map();
	best_match = {};

	if(verbose > 1)
		printf("\n");

	for(auto && src : track_map)
	{
		if(verbose)
			print_progress(src.first);

		for(unsigned int src_pos = 0; src_pos < src.second.get_event_count(); src_pos++)
		{
			Optimizer::Match match = find_match(src.first, src_pos);
			if(match.best_score() > best_match.best_score())
				best_match = match;
		}
	}

	if(verbose > 1)
	{
		printf("\nbest match: track %d, pos %d\n", best_match.track_id, best_match.position);
		printf("loop pos %d, loop len %d, score %d\n", best_match.loop_position, best_match.loop_length, best_match.loop_score());
		printf("sub length %d, repeat %d, sub score %d\n", best_match.sub_length, best_match.sub_repeats, best_match.sub_score);
	}

	if(best_match.best_score())
	{
		if(verbose > 1)
		{
			auto& ref = song->get_track(best_match.track_id).get_event(best_match.position).reference;
			printf("Reference line %d, col %d\n", ref->get_line() + 1, ref->get_column() + 1);
		}
		apply_match();
	}
}

void Optimizer::print_progress(int track_id)
{
	int best_score = best_match.best_score();

	if(best_score > top_score)
		top_score = best_match.best_score();

	long long range = (top_score - min_score);
	long long progress = (last_score > best_score) ? (top_score - last_score) : (top_score - best_score);
	if(!range)
	{
		range++;
		progress = 0;
	}
	else
	{
		// Exponential scale works better here
		progress = progress * progress * progress;
		if(progress)
			range *= range * range;
	}

	printf("\r%3lld%% pass%4d track%5d (%4d/%4d)", (progress * 100) / range, pass, track_id, best_score, last_score);
}

void Optimizer::apply_match()
{
	auto& src_track = song->get_track(best_match.track_id);
	auto& src_events = src_track.get_events();

	if(best_match.loop_score() < best_match.sub_score)
	{
		// Subroutine
		if(verbose > 1)
			printf("Create subroutine id %d\n", sub_id);
		auto& sub_track = song->make_track(sub_id);
		auto& sub_events = sub_track.get_events();

		// Copy source events
		sub_events.insert(sub_events.end(),
			src_events.begin() + best_match.position,
			src_events.begin() + best_match.position + best_match.sub_length);

		replace_with_subroutine(src_events, best_match.track_id, best_match.position, best_match.sub_length);

		find_subroutines();
		sub_id++;
	}
	else
	{
		// Loop
		uint32_t position = best_match.position;
		uint32_t length = best_match.loop_position - best_match.position;
		uint32_t repeats = (best_match.loop_length / length) + 1;
		uint32_t break_point = best_match.loop_length % length;
		if(break_point)
			repeats++;

		if(verbose > 1)
		{
			auto& ref = song->get_track(best_match.track_id).get_event(best_match.loop_position).reference;
			printf("Repeat reference line %d, col %d\n", ref->get_line() + 1, ref->get_column() + 1);
			printf("Create loop length=%d bp=%d repeats=%d\n", length, break_point, repeats);
		}

		// Delete the trailing data
		src_events.erase(src_events.begin() + best_match.loop_position,
			src_events.begin() + best_match.loop_position + best_match.loop_length);

		// Add loop commands
		std::shared_ptr<InputRef> reference = src_events[position].reference;
		add_event(src_events, best_match.track_id, position + length, Event::LOOP_END, repeats, reference);
		if(break_point)
			add_event(src_events, best_match.track_id, position + break_point, Event::LOOP_BREAK, 0, reference);
		add_event(src_events, best_match.track_id, position, Event::LOOP_START, 0, reference);
	}

#if 0
	auto track_map = song->get_track_map();
	for(auto && dst : track_map)
	{
		if(dst.first != 0)
			continue;
		for(unsigned int dst_pos = 0; dst_pos < dst.second.get_event_count(); dst_pos++)
		{
			printf("[%d,%d] = %d,%d,%d,%d\n", dst.first, dst_pos, dst.second.get_event(dst_pos).type, dst.second.get_event(dst_pos).param, dst.second.get_event(dst_pos).on_time, dst.second.get_event(dst_pos).off_time);
		}
	}
#endif
}

void Optimizer::find_subroutines()
{
	uint32_t src_track = best_match.track_id;
	uint32_t src_start = best_match.position;
	auto& track_map = song->get_track_map();

	for(auto && dst : track_map)
	{
		if(dst.first < src_track || dst.first == sub_id)
			continue;
		else if(dst.first == src_track)
		{
			for(unsigned int dst_pos = src_start + 1; dst_pos < dst.second.get_event_count(); dst_pos++)
			{
				unsigned int length = find_match_length(sub_id, 0, dst.first, dst_pos);
				if(length == best_match.sub_length)
					replace_with_subroutine(dst.second.get_events(), dst.first, dst_pos, length);
			}
		}
		else if(dst.first > src_track)
		{
			for(unsigned int dst_pos = 0; dst_pos < dst.second.get_event_count(); dst_pos++)
			{
				unsigned int length = find_match_length(sub_id, 0, dst.first, dst_pos);
				if(length == best_match.sub_length)
					replace_with_subroutine(dst.second.get_events(), dst.first, dst_pos, length);
			}
		}
	}
}

void Optimizer::replace_with_subroutine(std::vector<Event>& event_list, uint32_t track_id, uint32_t position, uint32_t length)
{
	if(verbose > 1)
		printf("replace subroutine track id=%d, %d, %d\n",track_id,position,length);

	// Copy reference + play time
	std::shared_ptr<InputRef> reference = event_list[position].reference;
	uint32_t play_time = event_list[position].play_time;

	// Replace occurence
	event_list.erase(event_list.begin() + position, event_list.begin() + position + length);
	event_list.insert(event_list.begin() + position, {Event::JUMP,sub_id,0,0,play_time,reference});

	// update stack depth event list (Not important what we set the replacement value to)
	auto& stack_list = stack_analyzer[track_id].event_list;
	stack_list.erase(stack_list.begin() + position, stack_list.begin() + position + length - 1);
}

void Optimizer::add_event(std::vector<Event>& event_list, uint32_t track_id, uint32_t position, Event::Type type, int16_t param, std::shared_ptr<InputRef>& reference)
{
	if(verbose > 1)
		printf("add event track id=%d, %d, type %d param %d\n",track_id,position,type,param);

	// Copy reference + play time
	uint32_t play_time = event_list[position].play_time;

	// Replace occurence
	event_list.insert(event_list.begin() + position, {type,param,0,0,play_time,reference});
}


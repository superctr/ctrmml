/*! \file src/optimizer.h
 *  \brief Track optimizer
 */
#ifndef OPTIMIZER_H
#define OPTIMIZER_H
#include "core.h"
#include "track.h"
#include <stack>
#include <memory>

class Optimizer;

/*! Track optimizer
 */
class Optimizer
{
	public:
		struct Stack_Analyzer
		{
			// returns drum mode status
			int analyze_track(Song&, Track& track, Optimizer& optimizer, int drum_mode);
			bool parsing = false; // avoid infinite nesting

			int16_t base_usage = 0; // set by calling tracks if subroutine
			int16_t max_usage = 0;
			std::vector<int16_t> event_list;
		};

		struct Match
		{
			uint16_t track_id = 0;
			uint32_t position = 0;

			//we can only find one loop match anyway
			uint32_t loop_position = 0;
			uint32_t loop_length = 0;

			uint32_t sub_length = 0;
			uint32_t sub_repeats = 0;
			int32_t sub_score = 0;

			inline int32_t loop_score()
			{
				return loop_length - 2;
			}
			inline int32_t best_score()
			{
				int32_t score = loop_score();
				return (sub_score > score) ? sub_score : score;
			}
			// internally used to determine the best score
			//<sub length,sub count>
		};

		Optimizer(Song& song, int verbose = 0);

		void optimize();
		void analyze_stack();

		unsigned int find_match_length(uint32_t src_track, uint32_t src_start, uint32_t dst_track, uint32_t dst_start, unsigned int* loop_length = nullptr);
		Match find_match(uint32_t src_track, uint32_t src_start);
		void find_best_match();
		void apply_match();

		void find_subroutines();
		void replace_with_subroutine(std::vector<Event>& event_list, uint32_t track_id, uint32_t position, uint32_t length);
		void add_event(std::vector<Event>& event_list, uint32_t track_id, uint32_t position, Event::Type type, int16_t param, std::shared_ptr<InputRef>& reference);

		void print_progress(int track_id);

		static const int max_stack_depth;
		static const int min_sub_score;
		static const int min_loop_score;
		static const int max_sub_stack;
		static const int max_loop_stack;

		int16_t sub_id;
		int pass;

		int verbose;
		int min_score;
		int last_score;
		int top_score;

		Song* song;
		Match best_match;
		std::map<int, Stack_Analyzer> stack_analyzer;
};

#endif

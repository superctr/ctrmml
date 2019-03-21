#include "playback.h"

int playback(struct pb_handler *pb_handler, int pb_count, int samples)
{
	int sample, status;
	for(sample=0; sample<samples; sample++)
	{
		int i;
		for(i=0; i<pb_count; i++)
		{
			pb_handler[i].delta_time += pb_handler[i].rate;
			while(pb_handler[i].delta_time > 1)
			{
				pb_handler[i].delta_time -= 1;
				status = pb_handler[i].callback(pb_handler[i].pb_state);
				if(status)
					return status;
			}
		}
	}
	return 0;
}

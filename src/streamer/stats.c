/** Global variables for statistics purposes
 *
 */
#include "stats.h"

int __vxg_stats_in_enabled = 0;
int __vxg_stats_in_video_bitrate = 0;
int __vxg_stats_in_audio_bitrate = 0;
int __vxg_stats_in_video_queue = 0;
int __vxg_stats_in_audio_queue = 0;
uint64_t __vxg_stats_in_video_timestamp = 0;
uint64_t __vxg_stats_in_audio_timestamp = 0;

int __vxg_stats_live_video_dropped = 0;
int __vxg_stats_live_audio_dropped = 0;
int __vxg_stats_live_enabled = 0;
int __vxg_stats_live_queue = 0;
int __vxg_stats_live_bitrate = 0;
int __vxg_stats_live_sent = 0;

int __vxg_stats_record_queue = 0;
int __vxg_stats_record_sent = 0;
int __vxg_stats_record_enabled = 0;

int __vxg_stats_backward_enabled = 0;
int __vxg_stats_backward_queue = 0;
int __vxg_stats_backward_received = 0;

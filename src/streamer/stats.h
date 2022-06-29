#ifndef __VXG_STREAMER_STATS
#define __VXG_STREAMER_STATS

#include <stdint.h>

/* Bitrate after RTP depayloader, i.e. encoders */
extern int __vxg_stats_in_enabled;
extern int __vxg_stats_in_video_bitrate;
extern uint64_t __vxg_stats_in_video_timestamp;
extern int __vxg_stats_in_audio_bitrate;
extern uint64_t __vxg_stats_in_audio_timestamp;

/* Queues after RTP depayloader, i.e. encoders */
extern int __vxg_stats_in_video_queue;
extern int __vxg_stats_in_audio_queue;

/* Stats for Live RTMP streaming */
extern int __vxg_stats_live_video_dropped;
extern int __vxg_stats_live_audio_dropped;
extern int __vxg_stats_live_enabled;
extern int __vxg_stats_live_queue;
extern int __vxg_stats_live_sent;
extern int __vxg_stats_live_bitrate;

/* Stats for RTMP source used in backward audio */
extern int __vxg_stats_backward_enabled;
extern int __vxg_stats_backward_queue;
extern int __vxg_stats_backward_received;

/* Stats for video recording(mp4 muxed fragments) */
extern int __vxg_stats_record_queue;
extern int __vxg_stats_record_sent;
extern int __vxg_stats_record_enabled;

/* System uptime */
extern int __vxg_stats_bootup_time;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
#endif

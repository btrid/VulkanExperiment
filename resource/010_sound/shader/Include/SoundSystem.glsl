#ifndef SOUND_SYSTEM_GLSL_
#define SOUND_SYSTEM_GLSL_

// Speaker Positions:
#define SPEAKER_FRONT_LEFT              0x1
#define SPEAKER_FRONT_RIGHT             0x2
#define SPEAKER_FRONT_CENTER            0x4
#define SPEAKER_LOW_FREQUENCY           0x8
#define SPEAKER_BACK_LEFT               0x10
#define SPEAKER_BACK_RIGHT              0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER    0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER   0x80
#define SPEAKER_BACK_CENTER             0x100
#define SPEAKER_SIDE_LEFT               0x200
#define SPEAKER_SIDE_RIGHT              0x400
#define SPEAKER_TOP_CENTER              0x800
#define SPEAKER_TOP_FRONT_LEFT          0x1000
#define SPEAKER_TOP_FRONT_CENTER        0x2000
#define SPEAKER_TOP_FRONT_RIGHT         0x4000
#define SPEAKER_TOP_BACK_LEFT           0x8000
#define SPEAKER_TOP_BACK_CENTER         0x10000
#define SPEAKER_TOP_BACK_RIGHT          0x20000

struct SoundFormat
{
	int		nChannels;          /* number of channels (i.e. mono, stereo...) */
	int		nSamplesPerSec;     /* sample rate */
	int		nAvgBytesPerSec;    /* for buffer estimation */
	int		nBlockAlign;        /* block size of data */

	int		wBitsPerSample;     /* number of bits per sample of mono data */
	int		_p1;
	int		_p2;
	int		_p3;

	// buffer format
	int frame_num;
	int samples_per_frame;// m_format.Format.nSamplesPerSec*m_format.Format.nChannels / 60.f
	uint m_buffer_length;
	int		_p23;
};

struct SoundPlayRequestData
{
	uint m_play_sound_id;
	uint m_current;
	uint m_time;
	uint m_is_playing;
	vec4 m_position; //!< w!=0.f ? 指向性 : ノット指向性
};
struct SoundPlayInfo
{
	uint m_sound_deltatime;
	uint m_write_start;
	uint _p2;
	uint _p3;

	vec4 m_listener;
	vec4 m_direction;

};

struct SoundInfo
{
	uint m_length;
	uint m_samples_per_sec;
	uint _p2;
	uint _p3;
};


#ifdef USE_SOUND_SYSTEM
#define SOUND_BANK_SIZE (1)
#define SOUND_REQUEST_SIZE (1)

layout(std430, set=USE_SOUND_SYSTEM, binding = 0) readonly restrict buffer SoundFormatUniform
{
	SoundFormat u_sound_format;
};
layout(std430, set=USE_SOUND_SYSTEM, binding = 1) readonly restrict buffer SoundPlayInfoUniform
{
	SoundPlayInfo u_play_info;
};
layout(std430, set=USE_SOUND_SYSTEM, binding = 2) restrict buffer SoundBuffer
{
	int b_sound_buffer[];
};

layout(std430, set=USE_SOUND_SYSTEM, binding = 10) restrict buffer SoundCounterBuffer
{
	uint b_active_id_counter;
	uint b_free_id_counter;
	uint b_request_counter;
};
layout(std430, set=USE_SOUND_SYSTEM, binding = 11) restrict buffer SoundActiveBuffer
{
	uint b_active_id_buffer[];	//!< 空いているSoundGenerateの番号リスト
};
layout(std430, set=USE_SOUND_SYSTEM, binding = 12) restrict buffer SoundFreeBuffer
{
	uint b_free_id_buffer[];
};
layout(std430, set=USE_SOUND_SYSTEM, binding = 13) restrict buffer SoundPlayingDataBuffer
{
	SoundPlayRequestData b_sound_playing_data[];
};
layout(std430, set=USE_SOUND_SYSTEM, binding = 14) restrict buffer SoundPlayRequestDataBuffer
{
	SoundPlayRequestData b_sound_request_data[];
};

layout(std140, set=USE_SOUND_SYSTEM, binding = 20) buffer SoundInfoUniform
{
	SoundInfo u_sound_info[SOUND_BANK_SIZE];
};

layout(std430, set=USE_SOUND_SYSTEM, binding = 21) buffer SoundDataBuffer
{
	int b_sound_data_buffer[];
};

#endif


#endif // SOUND_SYSTEM_GLSL_


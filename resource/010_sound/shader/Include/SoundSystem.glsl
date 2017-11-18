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
	int		_p22;
	int		_p23;
};

struct SoundPlayRequestData
{
	uint m_play_sound_id;
	uint m_current;
	uint _p2;
	uint _p3;
	vec4 m_position; //!< w!=0.f ? 指向性 : ノット指向性
};
struct SoundPlayInfo
{
	vec4 m_listener;
	vec4 m_direction;

	uint play_sound_num;
};

struct SoundInfo
{
	uint m_length;
	uint m_samples_per_sec;
};


#ifdef USE_SOUND_SYSTEM
layout(std140, set=USE_SOUND_SYSTEM, binding = 0) uniform SoundFormatUniform
{
	SoundFormat u_sound_format;
};
layout(std140, set=USE_SOUND_SYSTEM, binding = 1) uniform SoundPlayInfoUniform
{
	SoundPlayInfo u_play_info;
};
layout(std430, set=USE_SOUND_SYSTEM, binding = 2) buffer SoundBuffer
{
	uint b_sound_buffer[];
};

layout(std430, set=USE_SOUND_SYSTEM, binding = 20) buffer SoundAccumIndex
{
	uint b_accume_id_buffer_index;
};
layout(std140, set=USE_SOUND_SYSTEM, binding = 21) buffer SoundAccumBuffer
{
	uint b_accume_id_buffer[256];	//!< 空いているSoundGenerateの番号リスト
};
layout(std430, set=USE_SOUND_SYSTEM, binding = 22) buffer SoundPlayRequestDataBuffer
{
	SoundPlayRequestData u_sound_play_request_data[256];
};

layout(std140, set=USE_SOUND_SYSTEM, binding = 100) uniform SoundInfoUniform
{
	SoundInfo u_sound_info[64];
};

layout(std430, set=USE_SOUND_SYSTEM, binding = 101) buffer SoundDataBuffer
{
	uint b_sound_data_buffer[][64];
};

#endif


#endif // SOUND_SYSTEM_GLSL_


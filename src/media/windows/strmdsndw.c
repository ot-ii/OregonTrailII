#include "../strmdsnd.h"

#include <process.h>
#include <stddef.h>
#include <string.h>

#include <windows.h>

#include "../../base/file.h"
#include "../../base/memory.h"
#include "../xaudio2sink.h"

#define SND_SAMPLE_RATE 0x5622

struct strmd_voice {
	int32_t source;
	int32_t status;
	int32_t id;
	int32_t volume;
	struct file *file_handle;
	int32_t (*fill_cb)(void *arg);
	void *cb_arg;
	int32_t length;
	int32_t pos;
	void *buf_handle;
	uint8_t *data;
	int32_t total_bytes;
	bool loop;
	int32_t start_ms;
};

static CRITICAL_SECTION g_strmd_api_cs;
static CRITICAL_SECTION g_strmd_cb_cs;
static bool g_strmd_cs_ready;
static int32_t g_strmd_rate;
static int32_t g_strmd_channels;
static int32_t g_strmd_bits;
static uint8_t **g_strmd_databufs;
static struct strmd_voice g_strmd_voices[4];

static int32_t g_snd_num_buffers;
static int32_t g_snd_buffer_size;
static volatile int32_t g_snd_state;
static int32_t g_snd_pending_slot;
static int32_t g_snd_write_buf;
static int32_t g_snd_play_buf;
static int32_t g_snd_buffers_done;
static int32_t g_snd_next_id = 1;
static volatile bool g_snd_thread_stop;
static HANDLE g_snd_thread_done;

static void snd_mix_all(uint8_t *buf);
static bool snd_mix_slot(int32_t slot, uint8_t *buf);
static uint32_t snd_get_play_position(void);
static void fill_pcm_silence(uint8_t *buf, int32_t size);
static void memcpy_aligned(const uint8_t *src, uint8_t *dst, int32_t count);
static void mix_pcm8_saturating(const uint8_t *src, uint8_t *dst, int32_t count);
static int32_t snd_stream_fill_buffer(struct strmd_voice *v);
static int32_t snd_find_slot(int32_t handle);
static void snd_stream_close_source(struct strmd_voice *v);
static void snd_stream_start_thread(void);
static void snd_stream_stop_flag(void);
static void __cdecl snd_stream_thread(void *arg);

void StrmdSndInit(int32_t num_buffers, int32_t buffer_size)
{
	InitializeCriticalSection(&g_strmd_api_cs);
	InitializeCriticalSection(&g_strmd_cb_cs);
	g_strmd_cs_ready = true;
	g_snd_num_buffers = num_buffers;
	g_snd_buffer_size = buffer_size;

	g_strmd_rate = SND_SAMPLE_RATE;
	g_strmd_channels = 1;
	g_strmd_bits = 8;

	for (int32_t voice = 0; voice < 4; ++voice)
		g_strmd_voices[voice].source = 0;

	if (g_snd_num_buffers < 1)
		g_snd_num_buffers = 1;

	if (g_snd_buffer_size < 1)
		g_snd_buffer_size = 1;

	g_strmd_databufs = MemAlloc((size_t) g_snd_num_buffers, sizeof(uint8_t *));

	for (int32_t index = 0; index < g_snd_num_buffers; ++index) {
		g_strmd_databufs[index] = MemAlloc((size_t) g_snd_buffer_size, 1);
		fill_pcm_silence(g_strmd_databufs[index], g_snd_buffer_size);
	}

	g_snd_state = 2;
	snd_stream_start_thread();
}

void StrmdSndUnInit(void)
{
	snd_stream_stop_flag();

	if (g_snd_thread_done) {
		WaitForSingleObject(g_snd_thread_done, 5000);
		CloseHandle(g_snd_thread_done);
		g_snd_thread_done = NULL;
	}

	g_snd_state = 0;
	StrmdSndStopAll();
	EnterCriticalSection(&g_strmd_api_cs);
	Xa2Close();
	LeaveCriticalSection(&g_strmd_api_cs);

	for (int32_t index = 0; index < g_snd_num_buffers; ++index)
		MemFree(g_strmd_databufs[index]);

	MemFree(g_strmd_databufs);
	g_strmd_databufs = NULL;
	g_strmd_cs_ready = false;
	DeleteCriticalSection(&g_strmd_api_cs);
	DeleteCriticalSection(&g_strmd_cb_cs);
}

void StrmdSndPauseAll(void)
{
	EnterCriticalSection(&g_strmd_api_cs);

	if (g_snd_state == 3 || g_snd_state == 4) {
		Xa2Close();
		g_snd_state = 1;
	}

	LeaveCriticalSection(&g_strmd_api_cs);
}

void StrmdSndSetDevice(const wchar_t *id)
{
	if (g_snd_state == 0) {
		Xa2SetDevice(id);
		return;
	}

	EnterCriticalSection(&g_strmd_api_cs);
	Xa2SetDevice(id);

	if (g_snd_state == 3 || g_snd_state == 4) {
		Xa2Close();
		g_snd_state = 3;
		StrmdSndInterrupt();
	}

	LeaveCriticalSection(&g_strmd_api_cs);
}

void StrmdSndResumeAll(void)
{
	EnterCriticalSection(&g_strmd_api_cs);

	if (g_snd_state == 1) {
		g_snd_state = 3;
		StrmdSndInterrupt();
	}

	LeaveCriticalSection(&g_strmd_api_cs);
}

void StrmdSndInterrupt(void)
{
	int32_t i;

	if (g_snd_state == 0)
		return;

	EnterCriticalSection(&g_strmd_api_cs);

	switch (g_snd_state) {
		case 0:
		case 1:
		case 2:
			break;

		case 3:
			Xa2Open(g_strmd_rate, g_strmd_channels, g_strmd_bits);

			for (i = 0; i < g_snd_num_buffers; ++i) {
				snd_mix_all(g_strmd_databufs[i]);
				Xa2Submit(g_strmd_databufs[i], g_snd_buffer_size);
				g_snd_state = 4;
			}

			EnterCriticalSection(&g_strmd_cb_cs);
			g_snd_buffers_done = 0;
			g_snd_write_buf = 0;
			g_snd_play_buf = 0;
			LeaveCriticalSection(&g_strmd_cb_cs);
			break;

		case 4: {
			int32_t done_now = Xa2TakeBuffersDone();

			if (done_now > 0) {
				EnterCriticalSection(&g_strmd_cb_cs);
				g_snd_buffers_done = g_snd_buffers_done + done_now;
				g_snd_play_buf = (g_snd_play_buf + done_now) % g_snd_num_buffers;
				LeaveCriticalSection(&g_strmd_cb_cs);
			}

			if (g_snd_pending_slot != 0) {
				Xa2Pause();
				i = (g_snd_play_buf + 1) % g_snd_num_buffers;

				while (i != g_snd_write_buf) {
					fill_pcm_silence(g_strmd_databufs[i], g_snd_buffer_size);

					if (snd_mix_slot(g_snd_pending_slot - 1, g_strmd_databufs[i]))
						break;

					i = (i + 1) % g_snd_num_buffers;
				}

				g_snd_pending_slot = 0;
				Xa2Resume();
			}

			while (g_snd_buffers_done != 0) {
				snd_mix_all(g_strmd_databufs[g_snd_write_buf]);
				int32_t b = g_snd_write_buf;
				++g_snd_write_buf;
				Xa2Submit(g_strmd_databufs[b], g_snd_buffer_size);
				EnterCriticalSection(&g_strmd_cb_cs);

				if (g_snd_num_buffers == g_snd_write_buf)
					g_snd_write_buf = 0;

				--g_snd_buffers_done;
				LeaveCriticalSection(&g_strmd_cb_cs);
			}
			break;
		}
	}

	LeaveCriticalSection(&g_strmd_api_cs);
}

static void kick_voice(int32_t voice)
{
	if (g_snd_state == 2) {
		g_snd_state = 3;
		StrmdSndInterrupt();

	} else if (g_snd_state == 4) {
		g_snd_pending_slot = voice + 1;
		StrmdSndInterrupt();
	}
}

int32_t StrmdSndPlayFile(const char *path)
{
	for (int32_t voice = 0; voice < 4; ++voice) {
		struct strmd_voice *v = &g_strmd_voices[voice];

		if (v->source == 0) {
			EnterCriticalSection(&g_strmd_api_cs);
			v->file_handle = FileOpen(path, FILE_READ);
			v->length = 0;
			v->pos = 0;
			v->buf_handle = MemAlloc(0x4000, 1);
			v->data = v->buf_handle;
			v->source = 1;
			v->status = 1;
			++g_snd_next_id;
			v->id = g_snd_next_id;
			v->volume = 0;
			v->total_bytes = 0;
			v->start_ms = 0;
			LeaveCriticalSection(&g_strmd_api_cs);
			kick_voice(voice);

			return v->id;
		}
	}

	return -1;
}

int32_t StrmdSndPlayMem(void *data, int32_t length, bool loop)
{
	for (int32_t voice = 0; voice < 4; ++voice) {
		struct strmd_voice *v = &g_strmd_voices[voice];

		if (v->source == 0) {
			EnterCriticalSection(&g_strmd_api_cs);
			v->length = length;
			v->pos = 0;
			v->buf_handle = NULL;
			v->data = data;
			v->source = 2;
			v->status = 1;
			++g_snd_next_id;
			v->id = g_snd_next_id;
			v->volume = 0;
			v->total_bytes = 0;
			v->loop = loop;
			v->start_ms = 0;
			LeaveCriticalSection(&g_strmd_api_cs);
			kick_voice(voice);

			return v->id;
		}
	}

	return -1;
}

int32_t StrmdSndPlayCallback(void *data, int32_t length, int32_t (*fill_cb)(void *arg), void *cb_arg)
{
	for (int32_t voice = 0; voice < 4; ++voice) {
		struct strmd_voice *v = &g_strmd_voices[voice];

		if (v->source == 0) {
			EnterCriticalSection(&g_strmd_api_cs);
			v->cb_arg = cb_arg;
			v->fill_cb = fill_cb;
			v->length = length;
			v->pos = 0;
			v->buf_handle = NULL;
			v->data = data;
			v->source = 3;
			v->status = 1;
			++g_snd_next_id;
			v->id = g_snd_next_id;
			v->volume = 0;
			v->total_bytes = 0;
			v->start_ms = 0;
			LeaveCriticalSection(&g_strmd_api_cs);
			kick_voice(voice);

			return v->id;
		}
	}

	return -1;
}

int32_t StrmdSndStatus(int32_t handle)
{
	int32_t slot = snd_find_slot(handle);

	if (slot == -1)
		return 0;

	return g_strmd_voices[slot].status;
}

int32_t StrmdSndAmplitude(void)
{
	if (!g_strmd_databufs)
		return 0;

	int32_t sum = 0;
	int32_t buf_num = (int32_t) (Xa2SamplesPlayed() / (uint32_t) g_snd_buffer_size);
	const uint8_t *sample = g_strmd_databufs[buf_num % g_snd_num_buffers];

	for (int32_t i = 0; i < g_snd_buffer_size; ++i) {
		int32_t centred = (int32_t) *sample++ - 0x80;
		sum += centred < 0 ? -centred : centred;
	}

	return sum / g_snd_buffer_size;
}

void StrmdSndStop(int32_t handle)
{
	EnterCriticalSection(&g_strmd_api_cs);
	int32_t slot = snd_find_slot(handle);

	if (slot != -1)
		snd_stream_close_source(&g_strmd_voices[slot]);

	LeaveCriticalSection(&g_strmd_api_cs);
}

void StrmdSndPause(int32_t handle)
{
	EnterCriticalSection(&g_strmd_api_cs);
	int32_t slot = snd_find_slot(handle);

	if (slot != -1) {
		if (g_strmd_voices[slot].status == 1)
			g_strmd_voices[slot].status = 2;
	}

	LeaveCriticalSection(&g_strmd_api_cs);
}

void StrmdSndResume(int32_t handle)
{
	EnterCriticalSection(&g_strmd_api_cs);
	int32_t slot = snd_find_slot(handle);

	if (slot != -1) {
		if (g_strmd_voices[slot].status == 2) {
			g_strmd_voices[slot].status = 1;

			if (g_snd_state == 2) {
				g_snd_state = 3;

			} else if (g_snd_state == 4) {
				g_snd_pending_slot = slot + 1;
			}

			StrmdSndInterrupt();
		}
	}

	LeaveCriticalSection(&g_strmd_api_cs);
}

void StrmdSndVolume(int32_t handle, int32_t volume)
{
	uint8_t vol = (uint8_t) volume;
	int32_t slot = snd_find_slot(handle);

	if (slot == -1)
		return;

	struct strmd_voice *v = &g_strmd_voices[slot];

	if (vol > 0xBF) {
		v->volume = 0;

	} else if (vol > 0x60) {
		v->volume = 1;

	} else if (vol > 0x30) {
		v->volume = 2;

	} else if (vol > 0x18) {
		v->volume = 3;

	} else if (vol > 0x0C) {
		v->volume = 4;

	} else if (vol > 6) {
		v->volume = 5;

	} else if (vol > 3) {
		v->volume = 6;

	} else if (vol > 2) {
		v->volume = 7;

	} else {
		v->volume = 8;
	}
}

void StrmdSndStopAll(void)
{
	EnterCriticalSection(&g_strmd_api_cs);

	for (int32_t voice = 0; voice < 4; ++voice)
		snd_stream_close_source(&g_strmd_voices[voice]);

	LeaveCriticalSection(&g_strmd_api_cs);
}

int32_t StrmdSndGetPos(int32_t handle)
{
	int32_t slot = snd_find_slot(handle);

	if (slot == -1)
		return 0;

	if (g_strmd_voices[slot].start_ms == 0)
		return 0;

	int32_t elapsed = (int32_t) (Xa2SamplesPlayed() - (uint32_t) g_strmd_voices[slot].start_ms);

	if (elapsed < 0)
		return 0;

	return (int32_t) ((uint32_t) elapsed * 1000u) / SND_SAMPLE_RATE;
}

void StrmdSndLock(void)
{
	if (g_strmd_cs_ready)
		EnterCriticalSection(&g_strmd_api_cs);
}

void StrmdSndUnlock(void)
{
	if (g_strmd_cs_ready)
		LeaveCriticalSection(&g_strmd_api_cs);
}

static void snd_mix_all(uint8_t *buf)
{
	bool first = false;
	int32_t active = 0;

	for (int32_t index = 0; index < 4; ++index) {
		struct strmd_voice *v = &g_strmd_voices[index];

		if (v->source != 0 && v->status == 1) {
			int32_t remaining = g_snd_buffer_size;
			++active;

			if (v->start_ms == 0)
				v->start_ms = (int32_t) snd_get_play_position();

			do {
				int32_t chunk = v->length - v->pos;

				if (chunk > remaining)
					chunk = remaining;

				if (!first) {
					memcpy_aligned(v->data + v->pos, buf + g_snd_buffer_size - remaining, chunk);

				} else {
					mix_pcm8_saturating(v->data + v->pos, buf + g_snd_buffer_size - remaining, chunk);
				}

				v->pos += chunk;
				remaining -= chunk;

				if (v->pos >= v->length) {
					if (snd_stream_fill_buffer(v))
						break;
				}

			} while (remaining > 0 && v->length > 0);

			if (!first) {
				fill_pcm_silence(buf + g_snd_buffer_size - remaining, remaining);
				first = true;
			}
		}
	}

	if (active == 0)
		fill_pcm_silence(buf, g_snd_buffer_size);
}

static bool snd_mix_slot(int32_t slot, uint8_t *buf)
{
	bool ended = false;
	struct strmd_voice *v = &g_strmd_voices[slot];

	if (v->start_ms == 0)
		v->start_ms = (int32_t) snd_get_play_position();

	int32_t remaining = g_snd_buffer_size;

	do {
		int32_t chunk = v->length - v->pos;

		if (chunk > remaining)
			chunk = remaining;

		mix_pcm8_saturating(v->data + v->pos, buf + g_snd_buffer_size - remaining, chunk);
		v->pos += chunk;
		remaining -= chunk;

		if (v->pos >= v->length) {
			if (snd_stream_fill_buffer(v)) {
				ended = true;
				break;
			}
		}

	} while (remaining > 0 && v->length > 0);

	return ended;
}

static uint32_t snd_get_play_position(void)
{
	uint32_t played = Xa2SamplesPlayed();

	return played + (uint32_t) g_snd_buffer_size - played % (uint32_t) g_snd_buffer_size;
}

static void fill_pcm_silence(uint8_t *buf, int32_t size)
{
	if (size > 0)
		memset(buf, 0x80, (size_t) size);
}

static void memcpy_aligned(const uint8_t *src, uint8_t *dst, int32_t count)
{
	if (count > 0)
		memcpy(dst, src, (size_t) count);
}

static void mix_pcm8_saturating(const uint8_t *src, uint8_t *dst, int32_t count)
{
	for (int32_t i = 0; i < count; ++i) {
		int32_t sum = (int32_t) dst[i] + (int32_t) src[i];

		if (sum < 0x80) {
			dst[i] = 0;

		} else if (sum >= 0x180) {
			dst[i] = 0xFF;

		} else {
			dst[i] = (uint8_t) (sum - 0x80);
		}
	}
}

static int32_t snd_stream_fill_buffer(struct strmd_voice *v)
{
	v->pos = 0;

	switch (v->source) {
		case 0:
			return 1;

		case 1:
			v->length = v->file_handle ? FileRead(v->file_handle, v->data, 0x4000) : 0;

			if (v->length == 0)
				snd_stream_close_source(v);
			break;

		case 2:
			if (v->loop) {
				return 0;

			} else {
				v->length = 0;
				snd_stream_close_source(v);
				return 1;
			}

		case 3:
			v->length = v->fill_cb(v->cb_arg);

			if (v->length < 0) {
				snd_stream_close_source(v);
				return 1;

			} else if (v->length == 0) {
				return 0;
			}
			break;
	}

	if (v->length == 0)
		return 1;

	v->total_bytes += v->length;

	return 0;
}

static int32_t snd_find_slot(int32_t handle)
{
	if (handle == 0)
		return -1;

	for (int32_t voice = 0; voice < 4; ++voice) {
		if (g_strmd_voices[voice].id == handle)
			return voice;
	}

	return -1;
}

static void snd_stream_close_source(struct strmd_voice *v)
{
	switch (v->source) {
		case 0:
			break;

		case 1:
			FileClose(&v->file_handle);
			MemFree(v->buf_handle);
			v->buf_handle = NULL;
			v->source = 0;
			break;

		case 2:
		case 3:
			v->source = 0;
			break;
	}
}

static void snd_stream_start_thread(void)
{
	g_snd_thread_stop = false;

	if (!g_snd_thread_done) {
		g_snd_thread_done = CreateEventA(NULL, TRUE, FALSE, NULL);

	} else {
		ResetEvent(g_snd_thread_done);
	}

	_beginthread(snd_stream_thread, 0x800, NULL);
}

static void snd_stream_stop_flag(void)
{
	g_snd_thread_stop = true;
}

static void __cdecl snd_stream_thread(void *arg)
{
	(void) arg;

	while (!g_snd_thread_stop) {
		if (g_snd_state == 4)
			StrmdSndInterrupt();

		Sleep(0x1F4);
	}

	if (g_snd_thread_done)
		SetEvent(g_snd_thread_done);

	_endthread();
}

#define COBJMACROS

#include "../xaudio2sink.h"

#include <stdlib.h>
#include <wchar.h>

#include <windows.h>
#include <xaudio2.h>
#include <mmdeviceapi.h>
#include <propsys.h>

#include "../../base/ini.h"

struct sink_callback {
	IXAudio2VoiceCallback iface;
	volatile LONG done;
	volatile LONG queued;
};

static void STDMETHODCALLTYPE cb_pass_start(IXAudio2VoiceCallback *self, UINT32 bytes)
{
	(void) self;
	(void) bytes;
}

static void STDMETHODCALLTYPE cb_pass_end(IXAudio2VoiceCallback *self)
{
	(void) self;
}

static void STDMETHODCALLTYPE cb_stream_end(IXAudio2VoiceCallback *self)
{
	(void) self;
}

static void STDMETHODCALLTYPE cb_buffer_start(IXAudio2VoiceCallback *self, void *context)
{
	(void) self;
	(void) context;
}

static void STDMETHODCALLTYPE cb_buffer_end(IXAudio2VoiceCallback *self, void *context)
{
	struct sink_callback *cb = (struct sink_callback *) self;
	(void) context;

	InterlockedIncrement(&cb->done);
	InterlockedDecrement(&cb->queued);
}

static void STDMETHODCALLTYPE cb_loop_end(IXAudio2VoiceCallback *self, void *context)
{
	(void) self;
	(void) context;
}

static void STDMETHODCALLTYPE cb_voice_error(IXAudio2VoiceCallback *self, void *context, HRESULT error)
{
	(void) self;
	(void) context;
	(void) error;
}

static IXAudio2VoiceCallbackVtbl g_callback_vtbl = {
	cb_pass_start,
	cb_pass_end,
	cb_stream_end,
	cb_buffer_start,
	cb_buffer_end,
	cb_loop_end,
	cb_voice_error,
};

static struct sink_callback g_callback = {{&g_callback_vtbl}, 0, 0};

static IXAudio2 *g_engine;
static IXAudio2MasteringVoice *g_master;
static IXAudio2SourceVoice *g_source;
static bool g_com_initialised;
static bool g_paused;

struct device {
	wchar_t id[256];
	wchar_t name[128];
};

static struct device g_devices[XA2_MAX_DEVICES];
static int32_t g_device_count;
static wchar_t g_device[256];
static bool g_device_loaded;

static const CLSID g_clsid_mm_device_enumerator =
	{0xbcde0395, 0xe52f, 0x467c, {0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e}};
static const IID g_iid_imm_device_enumerator =
	{0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}};
static const PROPERTYKEY g_friendly_name =
	{{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};

static void load_device_setting(void)
{
	char text[256];

	if (g_device_loaded)
		return;

	g_device_loaded = true;
	IniGetString("sound", "device", "", text, sizeof(text));

	if (!MultiByteToWideChar(CP_UTF8, 0, text, -1, g_device, (int) _countof(g_device)))
		g_device[0] = 0;
}

int32_t Xa2EnumDevices(void)
{
	IMMDeviceEnumerator *en = NULL;
	IMMDeviceCollection *list = NULL;
	UINT count = 0;

	g_device_count = 0;
	HRESULT co = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	if (SUCCEEDED(CoCreateInstance(&g_clsid_mm_device_enumerator, NULL, CLSCTX_ALL, &g_iid_imm_device_enumerator,
			(void **) &en))
		&& SUCCEEDED(IMMDeviceEnumerator_EnumAudioEndpoints(en, eRender, DEVICE_STATE_ACTIVE, &list))
		&& SUCCEEDED(IMMDeviceCollection_GetCount(list, &count))) {
		for (UINT i = 0; i < count && g_device_count < XA2_MAX_DEVICES; ++i) {
			IMMDevice *dev = NULL;
			IPropertyStore *props = NULL;
			LPWSTR id = NULL;
			struct device *d = &g_devices[g_device_count];

			if (FAILED(IMMDeviceCollection_Item(list, i, &dev)))
				continue;

			if (SUCCEEDED(IMMDevice_GetId(dev, &id)) && wcslen(id) < _countof(d->id)) {
				PROPVARIANT name;

				wcsncpy_s(d->id, _countof(d->id), id, _TRUNCATE);
				wcsncpy_s(d->name, _countof(d->name), id, _TRUNCATE);
				PropVariantInit(&name);

				if (SUCCEEDED(IMMDevice_OpenPropertyStore(dev, STGM_READ, &props))
					&& SUCCEEDED(IPropertyStore_GetValue(props, &g_friendly_name, &name)) && name.vt == VT_LPWSTR)
					wcsncpy_s(d->name, _countof(d->name), name.pwszVal, _TRUNCATE);

				PropVariantClear(&name);
				++g_device_count;
			}

			CoTaskMemFree(id);

			if (props)
				IPropertyStore_Release(props);

			IMMDevice_Release(dev);
		}
	}

	if (list)
		IMMDeviceCollection_Release(list);

	if (en)
		IMMDeviceEnumerator_Release(en);

	if (SUCCEEDED(co))
		CoUninitialize();

	return g_device_count;
}

const wchar_t *Xa2DeviceId(int32_t index)
{
	return index >= 0 && index < g_device_count ? g_devices[index].id : L"";
}

const wchar_t *Xa2DeviceName(int32_t index)
{
	return index >= 0 && index < g_device_count ? g_devices[index].name : L"";
}

const wchar_t *Xa2GetDevice(void)
{
	load_device_setting();

	return g_device;
}

void Xa2SetDevice(const wchar_t *id)
{
	char text[256];

	g_device_loaded = true;

	if (!id || wcslen(id) >= _countof(g_device))
		id = L"";

	wcsncpy_s(g_device, _countof(g_device), id, _TRUNCATE);

	if (!WideCharToMultiByte(CP_UTF8, 0, g_device, -1, text, (int) sizeof(text), NULL, NULL))
		text[0] = 0;

	IniSetString("sound", "device", text);
}

int32_t Xa2Open(int32_t samples_per_sec, int32_t channels, int32_t bits_per_sample)
{
	WAVEFORMATEX fmt;
	HRESULT hr;

	if (g_engine)
		return 0;

	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	g_com_initialised = SUCCEEDED(hr);

	hr = XAudio2Create(&g_engine, 0, XAUDIO2_DEFAULT_PROCESSOR);

	if (FAILED(hr)) {
		g_engine = NULL;
		Xa2Close();
		return 1;
	}

	load_device_setting();
	hr = E_FAIL;

	if (g_device[0])
		hr = IXAudio2_CreateMasteringVoice(g_engine, &g_master, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE,
			0, g_device, NULL, AudioCategory_GameEffects);

	if (FAILED(hr))
		hr = IXAudio2_CreateMasteringVoice(g_engine, &g_master, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE,
			0, NULL, NULL, AudioCategory_GameEffects);

	if (FAILED(hr)) {
		g_master = NULL;
		Xa2Close();
		return 2;
	}

	ZeroMemory(&fmt, sizeof(fmt));
	fmt.wFormatTag = WAVE_FORMAT_PCM;
	fmt.nChannels = (WORD) channels;
	fmt.nSamplesPerSec = (DWORD) samples_per_sec;
	fmt.wBitsPerSample = (WORD) bits_per_sample;
	fmt.nBlockAlign = (WORD) (channels * bits_per_sample / 8);
	fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
	fmt.cbSize = 0;

	hr = IXAudio2_CreateSourceVoice(g_engine, &g_source, &fmt, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &g_callback.iface, NULL,
		NULL);

	if (FAILED(hr)) {
		g_source = NULL;
		Xa2Close();
		return 3;
	}

	g_callback.done = 0;
	g_callback.queued = 0;
	g_paused = false;

	if (FAILED(IXAudio2SourceVoice_Start(g_source, 0, XAUDIO2_COMMIT_NOW))) {
		Xa2Close();
		return 4;
	}

	return 0;
}

void Xa2Close(void)
{
	if (g_source) {
		IXAudio2SourceVoice_Stop(g_source, 0, XAUDIO2_COMMIT_NOW);
		IXAudio2SourceVoice_FlushSourceBuffers(g_source);
		IXAudio2SourceVoice_DestroyVoice(g_source);
		g_source = NULL;
	}

	if (g_master) {
		IXAudio2MasteringVoice_DestroyVoice(g_master);
		g_master = NULL;
	}

	if (g_engine) {
		IXAudio2_Release(g_engine);
		g_engine = NULL;
	}

	if (g_com_initialised) {
		CoUninitialize();
		g_com_initialised = false;
	}

	g_callback.done = 0;
	g_callback.queued = 0;
	g_paused = false;
}

bool Xa2IsOpen(void)
{
	return g_source != NULL;
}

int32_t Xa2Submit(const void *data, int32_t bytes)
{
	XAUDIO2_BUFFER buf;

	if (!g_source || bytes <= 0)
		return 1;

	ZeroMemory(&buf, sizeof(buf));
	buf.AudioBytes = (UINT32) bytes;
	buf.pAudioData = (const BYTE *) data;

	InterlockedIncrement(&g_callback.queued);

	if (FAILED(IXAudio2SourceVoice_SubmitSourceBuffer(g_source, &buf, NULL))) {
		InterlockedDecrement(&g_callback.queued);
		return 2;
	}

	return 0;
}

int32_t Xa2TakeBuffersDone(void)
{
	if (!g_source)
		return 0;

	return (int32_t) InterlockedExchange(&g_callback.done, 0);
}

int32_t Xa2QueuedBuffers(void)
{
	if (!g_source)
		return 0;

	return (int32_t) InterlockedCompareExchange(&g_callback.queued, 0, 0);
}

uint32_t Xa2SamplesPlayed(void)
{
	XAUDIO2_VOICE_STATE state;

	if (!g_source)
		return 0;

	IXAudio2SourceVoice_GetState(g_source, &state, 0);

	return (uint32_t) state.SamplesPlayed;
}

void Xa2Pause(void)
{
	if (g_source && !g_paused) {
		IXAudio2SourceVoice_Stop(g_source, 0, XAUDIO2_COMMIT_NOW);
		g_paused = true;
	}
}

void Xa2Resume(void)
{
	if (g_source && g_paused) {
		IXAudio2SourceVoice_Start(g_source, 0, XAUDIO2_COMMIT_NOW);
		g_paused = false;
	}
}

void Xa2Flush(void)
{
	if (!g_source)
		return;

	IXAudio2SourceVoice_Stop(g_source, 0, XAUDIO2_COMMIT_NOW);
	IXAudio2SourceVoice_FlushSourceBuffers(g_source);

	for (int32_t tries = 0; tries < 200; ++tries) {
		XAUDIO2_VOICE_STATE state;
		IXAudio2SourceVoice_GetState(g_source, &state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

		if (state.BuffersQueued == 0)
			break;

		Sleep(1);
	}

	InterlockedExchange(&g_callback.done, 0);
	InterlockedExchange(&g_callback.queued, 0);
	g_paused = true;
}

static IXAudio2 *g_clip_engine;
static IXAudio2MasteringVoice *g_clip_master;
static IXAudio2SourceVoice *g_clip_voice;
static wchar_t g_clip_device[256];
static int16_t *g_clip_pcm;
static int32_t g_clip_frames;
static int32_t g_clip_rate;
static int32_t g_clip_channels;
static float g_clip_volume = 1.0f;

static void clip_close_engine(void)
{
	Xa2ClipStop();

	if (g_clip_master) {
		IXAudio2MasteringVoice_DestroyVoice(g_clip_master);
		g_clip_master = NULL;
	}

	if (g_clip_engine) {
		IXAudio2_Release(g_clip_engine);
		g_clip_engine = NULL;
	}
}

static bool clip_engine(void)
{
	HRESULT hr;

	load_device_setting();

	if (g_clip_engine && !wcscmp(g_clip_device, g_device))
		return true;

	clip_close_engine();

	if (FAILED(XAudio2Create(&g_clip_engine, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
		g_clip_engine = NULL;
		return false;
	}

	hr = E_FAIL;

	if (g_device[0])
		hr = IXAudio2_CreateMasteringVoice(g_clip_engine, &g_clip_master, XAUDIO2_DEFAULT_CHANNELS,
			XAUDIO2_DEFAULT_SAMPLERATE, 0, g_device, NULL, AudioCategory_GameEffects);

	if (FAILED(hr))
		hr = IXAudio2_CreateMasteringVoice(g_clip_engine, &g_clip_master, XAUDIO2_DEFAULT_CHANNELS,
			XAUDIO2_DEFAULT_SAMPLERATE, 0, NULL, NULL, AudioCategory_GameEffects);

	if (FAILED(hr)) {
		g_clip_master = NULL;
		clip_close_engine();
		return false;
	}

	wcsncpy_s(g_clip_device, _countof(g_clip_device), g_device, _TRUNCATE);

	return true;
}

bool Xa2ClipLoad(const void *pcm, int32_t bytes, int32_t samples_per_sec, int32_t channels, int32_t bits_per_sample)
{
	Xa2ClipUnload();

	if (!pcm || bytes <= 0 || samples_per_sec <= 0 || channels < 1 || channels > 2 ||
		(bits_per_sample != 8 && bits_per_sample != 16) || !clip_engine())
		return false;

	int32_t samples = bytes / (bits_per_sample / 8);
	samples -= samples % channels;

	if (samples <= 0)
		return false;

	g_clip_pcm = malloc((size_t) samples * sizeof(int16_t));

	if (!g_clip_pcm)
		return false;

	const uint8_t *src = pcm;

	for (int32_t i = 0; i < samples; ++i)
		g_clip_pcm[i] = bits_per_sample == 8 ? (int16_t) ((src[i] - 128) * 256) :
			(int16_t) (src[2 * i] | src[2 * i + 1] << 8);

	g_clip_frames = samples / channels;
	g_clip_rate = samples_per_sec;
	g_clip_channels = channels;

	return true;
}

void Xa2ClipPlay(int32_t ms)
{
	WAVEFORMATEX fmt;
	XAUDIO2_BUFFER buf;

	Xa2ClipStop();

	if (!g_clip_pcm || !clip_engine())
		return;

	int64_t first = ms > 0 ? (int64_t) ms * g_clip_rate / 1000 : 0;

	if (first >= g_clip_frames)
		return;

	ZeroMemory(&fmt, sizeof(fmt));
	fmt.wFormatTag = WAVE_FORMAT_PCM;
	fmt.nChannels = (WORD) g_clip_channels;
	fmt.nSamplesPerSec = (DWORD) g_clip_rate;
	fmt.wBitsPerSample = 16;
	fmt.nBlockAlign = (WORD) (2 * g_clip_channels);
	fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

	if (FAILED(IXAudio2_CreateSourceVoice(g_clip_engine, &g_clip_voice, &fmt, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL,
			NULL, NULL))) {
		g_clip_voice = NULL;
		return;
	}

	ZeroMemory(&buf, sizeof(buf));
	buf.pAudioData = (const BYTE *) (g_clip_pcm + first * g_clip_channels);
	buf.AudioBytes = (UINT32) ((g_clip_frames - first) * fmt.nBlockAlign);
	buf.Flags = XAUDIO2_END_OF_STREAM;
	IXAudio2SourceVoice_SetVolume(g_clip_voice, g_clip_volume, XAUDIO2_COMMIT_NOW);

	if (FAILED(IXAudio2SourceVoice_SubmitSourceBuffer(g_clip_voice, &buf, NULL)) ||
		FAILED(IXAudio2SourceVoice_Start(g_clip_voice, 0, XAUDIO2_COMMIT_NOW)))
		Xa2ClipStop();
}

void Xa2ClipStop(void)
{
	if (g_clip_voice) {
		IXAudio2SourceVoice_DestroyVoice(g_clip_voice);
		g_clip_voice = NULL;
	}
}

void Xa2ClipVolume(int32_t volume)
{
	g_clip_volume = volume <= 0 ? 0.0f : volume >= 1000 ? 1.0f : (float) volume / 1000.0f;

	if (g_clip_voice)
		IXAudio2SourceVoice_SetVolume(g_clip_voice, g_clip_volume, XAUDIO2_COMMIT_NOW);
}

void Xa2ClipUnload(void)
{
	Xa2ClipStop();
	free(g_clip_pcm);
	g_clip_pcm = NULL;
	g_clip_frames = 0;
}

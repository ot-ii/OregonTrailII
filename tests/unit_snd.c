//

#include "testframework.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <string.h>

#include "../src/app/otiiapp.h"
#include "../src/base/byteorder.h"
#include "../src/base/clock.h"
#include "../src/base/file.h"
#include "../src/base/memory.h"
#include "../src/base/str.h"
#include "../src/data/assets.h"
#include "../src/data/decode.h"
#include "../src/data/gfxdev.h"
#include "../src/data/mecclib.h"
#include "../src/data/rsrc.h"
#include "../src/media/filstrm.h"
#include "../src/media/sndunpack.h"
#include "../src/media/sndupkstrm.h"
#include "../src/media/strmdsnd.h"
#include "../src/media/winmixedstreamedsnd.h"
#include "testsupport.h"

#define MIXER_PIECE 0x1000
#define MAX_IDS 64
#define MAX_RANGES 512

struct range {
	int32_t track;
	int32_t start_cue;
	int32_t loop_cue;
	int32_t end_cue;
};

struct id_list {
	const char *type;
	int32_t *ids;
	int32_t n, max;
};

static void collect_id(const char *name, int32_t id, int32_t storage, int32_t size, int64_t offset, void *opaque)
{
	struct id_list *l = opaque;
	int32_t j;

	(void) storage;
	(void) size;
	(void) offset;
	if (!StrEqualNoCase(name, l->type) || l->n >= l->max)
		return;
	for (j = 0; j < l->n && l->ids[j] != id; ++j)
		;
	if (j == l->n)
		l->ids[l->n++] = id;
}

static int32_t ids_of(const char *type, int32_t *ids, int32_t max)
{
	struct id_list l = {type, ids, 0, max};

	for (int32_t i = 0; i < AssetsSourceCount(g_data_lib); ++i) {
		struct mecclib *lib = AssetsSourceMeccLib(g_data_lib, i);
		if (lib && MeccLibIsAvailable(lib))
			MeccLibEnumerate(lib, collect_id, &l);
	}
	return l.n;
}

static int32_t music_ranges(struct range *out, int32_t max)
{
	int32_t ids[MAX_IDS];
	int32_t n = 0, count;

	count = ids_of("Cue", ids, MAX_IDS);
	for (int32_t i = 0; i < count; ++i) {
		struct rsrc *r = AssetsGetElement(g_data_lib, "Cue", ids[i], false);
		int32_t track = 0, entries = 0;

		if (!r)
			continue;
		RsrcLoadAndUnpack(r);
		if (RsrcSize(r) >= 4)
			CueListReadHeader(RsrcData(r), &track, &entries);
		for (int32_t k = 0; k < entries && 4 + 8 * (k + 1) <= RsrcSize(r) && n < max; ++k) {
			struct cue_entry e = CueListReadEntry(RsrcData(r), k);
			struct range g = {(int16_t) track, e.start_cue, e.loop_cue, e.end_cue};
			out[n++] = g;
		}
		RsrcDestroy(&r);
	}

	count = ids_of("Pool", ids, MAX_IDS);
	for (int32_t i = 0; i < count; ++i) {
		struct rsrc *r = AssetsGetElement(g_data_lib, "Pool", ids[i], false);
		int32_t entries;

		if (!r)
			continue;
		RsrcLoadAndUnpack(r);
		entries = RsrcSize(r) >= 2 ? SndPoolReadCount(RsrcData(r)) : 0;
		for (int32_t k = 0; k < entries && 2 + 10 * (k + 1) <= RsrcSize(r) && n < max; ++k) {
			uint32_t track;
			struct cue_entry e;
			SndPoolReadEntry(RsrcData(r), k, &track, &e);
			struct range g = {(int16_t) track, e.start_cue, e.loop_cue, e.end_cue};
			out[n++] = g;
		}
		RsrcDestroy(&r);
	}
	return n;
}

static struct snd_upk_strm *open_track(int32_t id)
{
	char path[1024];
	int64_t where = 0;
	int32_t size = 0;
	struct file *f;

	if (!AssetsGetElementInfo(g_data_lib, "Csnd", id, path, sizeof(path), &where, &size))
		return NULL;
	f = FileOpen(path, FILE_READ);
	if (!f)
		return NULL;
	return SndUpkStrmCreate((struct stream *) FilStrmCreate(f, (int32_t) where, size, 0x8000, 70));
}

static void close_track(struct snd_upk_strm **u)
{
	if (*u)
		ObjectDelete(*u);
	*u = NULL;
}

static int32_t cue_offset(struct rsrc *sndq, int32_t cue)
{
	int32_t count = (int16_t) ((uint32_t) RsrcSize(sndq) >> 3);
	int32_t offset = -1;

	for (int32_t i = 0; i < count; ++i)
		if (ReadLe32(RsrcData(sndq) + 8 * i + 4) == cue)
			offset = ReadLe32(RsrcData(sndq) + 8 * i);
	return offset;
}

static uint8_t *decode_whole(int32_t id, int32_t *total)
{
	struct snd_upk_strm *u = open_track(id);
	struct stream *st;
	uint8_t *pcm;
	int32_t got = 0;

	*total = 0;
	if (!u || !u->inner)
		return NULL;
	st = &u->base.base;
	*total = STREAM_OPS(st)->Size(st);
	pcm = MemAlloc((size_t) *total + 1, 1);
	while (got < *total) {
		int32_t len = *total - got < MIXER_PIECE ? *total - got : MIXER_PIECE;
		int32_t status = STREAM_OPS(st)->Get(st, pcm + got, &len);
		got += len;
		if (status || !len)
			break;
	}
	close_track(&u);
	CHECK_MSG(got == *total, "track %d: %d of %d samples from the start", id, got, *total);
	return pcm;
}

static int32_t play_and_compare(struct snd_upk_strm *u, const uint8_t *pcm, int32_t total, int32_t start,
	int32_t loop_start, int32_t end, int32_t pieces, int32_t *read)
{
	struct snd_strm *ss = &u->base;
	struct stream *st = &u->base.base;
	uint8_t *buf = MemAlloc(MIXER_PIECE, 1);
	bool looping = loop_start > 0;
	int32_t pos = start, first_bad = -1;

	*read = 0;
	SND_STRM_OPS(ss)->SetSelection(ss, start, looping ? loop_start : -1, end, 0);
	STREAM_OPS(st)->Seek(st, 0);
	for (int32_t piece = 0; piece < pieces && first_bad < 0; ++piece) {
		int32_t len = MIXER_PIECE;
		int32_t status = STREAM_OPS(st)->Get(st, buf, &len);

		if (status == 1 && looping) {
			STREAM_OPS(st)->Seek(st, 0);
			if (!len) {
				len = MIXER_PIECE;
				STREAM_OPS(st)->Get(st, buf, &len);
			}
		}
		for (int32_t i = 0; i < len; ++i, ++pos) {
			if (pos >= end && looping)
				pos = loop_start;
			if (pos >= end || pos >= total || buf[i] != pcm[pos]) {
				first_bad = pos;
				break;
			}
		}
		*read += len;
		if ((status == 1 && !looping) || !len)
			break;
	}
	MemFree(buf);
	return first_bad;
}

struct track_pcm {
	int32_t id;
	int32_t total;
	uint8_t *pcm;
};

static int32_t distinct_tracks(const struct range *ranges, int32_t n, struct track_pcm *tracks, int32_t max)
{
	int32_t count = 0;

	for (int32_t i = 0; i < n; ++i) {
		int32_t j;
		for (j = 0; j < count && tracks[j].id != ranges[i].track; ++j)
			;
		if (j == count && count < max) {
			tracks[count].id = ranges[i].track;
			tracks[count].pcm = decode_whole(ranges[i].track, &tracks[count].total);
			++count;
		}
	}
	return count;
}

static void free_tracks(struct track_pcm *tracks, int32_t n)
{
	for (int32_t i = 0; i < n; ++i)
		if (tracks[i].pcm)
			MemFree(tracks[i].pcm);
}

TEST(unit_snd, cue_ranges_match_whole_decode)
{
	static struct range ranges[MAX_RANGES];
	static struct track_pcm tracks[MAX_IDS];
	int32_t n = music_ranges(ranges, MAX_RANGES);
	int32_t ntracks, before = DecodeOutOfRangeReads();

	CHECK(n > 0);
	ntracks = distinct_tracks(ranges, n, tracks, MAX_IDS);
	TNote("%d cue ranges over %d tracks", n, ntracks);

	for (int32_t t = 0; t < ntracks; ++t) {
		struct rsrc *sndq = AssetsGetElement(g_data_lib, "SndQ", tracks[t].id, false);
		struct snd_upk_strm *shared;

		if (!tracks[t].pcm || !sndq) {
			if (sndq)
				RsrcDestroy(&sndq);
			continue;
		}
		shared = open_track(tracks[t].id);
		for (int32_t i = 0; i < n; ++i) {
			int32_t start, loop, end, read, bad;
			struct snd_upk_strm *own;

			if (ranges[i].track != tracks[t].id)
				continue;
			start = cue_offset(sndq, ranges[i].start_cue);
			loop = cue_offset(sndq, ranges[i].loop_cue);
			end = cue_offset(sndq, ranges[i].end_cue);
			if (start < 0 || end < 0 || start >= end)
				continue;

			own = open_track(tracks[t].id);
			bad = play_and_compare(own, tracks[t].pcm, tracks[t].total, start, loop, end, 400, &read);
			CHECK_MSG(bad < 0, "track %d cues %d/%d/%d (%d..%d loop %d): sample %d differs after %d read", tracks[t].id,
				ranges[i].start_cue, ranges[i].loop_cue, ranges[i].end_cue, start, end, loop, bad, read);
			close_track(&own);

			bad = play_and_compare(shared, tracks[t].pcm, tracks[t].total, start, loop, end, 40, &read);
			CHECK_MSG(bad < 0, "track %d (one stream) cues %d/%d/%d: sample %d differs after %d read", tracks[t].id,
				ranges[i].start_cue, ranges[i].loop_cue, ranges[i].end_cue, bad, read);
		}
		close_track(&shared);
		RsrcDestroy(&sndq);
	}
	CHECK_EQ(DecodeOutOfRangeReads() - before, 0);
	free_tracks(tracks, ntracks);
}

TEST(unit_snd, seek_to_every_key_frame)
{
	static struct range ranges[MAX_RANGES];
	static struct track_pcm tracks[MAX_IDS];
	int32_t n = music_ranges(ranges, MAX_RANGES);
	int32_t ntracks = distinct_tracks(ranges, n, tracks, MAX_IDS);
	int32_t seeks = 0, failures = 0, before = DecodeOutOfRangeReads();

	for (int32_t t = 0; t < ntracks; ++t) {
		struct snd_upk_strm *u = tracks[t].pcm ? open_track(tracks[t].id) : NULL;

		if (!u || !u->inner) {
			close_track(&u);
			continue;
		}
		for (int32_t k = 0; k < u->inner->keyframe_count; ++k) {
			int32_t at = u->inner->keyframes[k].sample_pos;

			for (int32_t pass = 0; pass < 2; ++pass) {
				int32_t pos = pass ? at + 1000 : at, read, bad;

				if (pos >= tracks[t].total)
					continue;
				bad = play_and_compare(u, tracks[t].pcm, tracks[t].total, pos, -1, tracks[t].total, 4, &read);
				++seeks;
				if (bad >= 0 && failures++ < 10)
					CHECK_MSG(false, "track %d key frame %d (sample %d%s): sample %d differs", tracks[t].id, k, at,
						pass ? " + 1000" : "", bad);
			}
		}
		close_track(&u);
	}
	TNote("%d seeks, %d failed", seeks, failures);
	CHECK_EQ(failures, 0);
	CHECK(seeks > 0);
	CHECK_EQ(DecodeOutOfRangeReads() - before, 0);
	free_tracks(tracks, ntracks);
}

TEST(unit_snd, back_reference_past_buffer)
{
	static const uint8_t ref_far[] = {0xBF, 0xFF, 0xFF, 0xC0};
	struct decode_table table;
	uint8_t out[64];
	uint32_t extra = 7;
	uint8_t *src = MemAlloc(sizeof(ref_far), 1);
	int32_t before = DecodeOutOfRangeReads();

	memcpy(src, ref_far, sizeof(ref_far));
	memset(&table, 0, sizeof(table));
	table.count = 256;
	for (int32_t i = 0; i < 256; ++i)
		table.table[i] = (uint8_t) (0x80 + i);
	memset(out, 0x55, sizeof(out));
	CHECK_EQ(DecodeImageBlock(&table, src, (int32_t) sizeof(ref_far), 0, out, &extra), 26);
	CHECK_EQ(extra, 0);
	CHECK(DecodeOutOfRangeReads() > before);
	for (int32_t i = 0; i < 64; ++i)
		CHECK_MSG(out[i] == 0x80, "sample %d is %d", i, out[i]);
	MemFree(src);

	static const uint8_t frame[] = {12, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 0xBF, 0xFF, 0xFF, 0xC0};
	uint8_t *data = MemAlloc(sizeof(frame), 1);
	uint8_t pixels[64];

	memcpy(data, frame, sizeof(frame));
	memset(pixels, 0x55, sizeof(pixels));
	DecodeFrame(data, (int32_t) sizeof(frame), 8, 8, pixels, 8, DECODE_OPAQUE);
	CHECK_EQ(pixels[0], 0);
	CHECK_EQ(pixels[63], 0);
	MemFree(data);
}

struct mixer_race {
	struct win_mixed_streamed_snd *snd;
	struct snd_upk_strm *upk;
	const uint8_t *pcm;
	int32_t total;
	volatile bool stop;
	int32_t fills;
	int32_t bad_fills;
	int32_t first_bad;
};

static void mixer_thread(void *arg)
{
	struct mixer_race *r = arg;
	struct stream *st = &r->upk->base.base;

	while (!r->stop) {
		StrmdSndLock();
		int32_t at = st->start_pos + r->upk->cur_pos;
		bool at_end = r->upk->eof_flag;
		int32_t filled = WinMixedStreamedSndFillBuffer(r->snd);
		if (!at_end && filled > 0 && (at < 0 || at + filled > r->total ||
			memcmp(r->snd->play_ptr, r->pcm + at, (size_t) filled) != 0)) {
			if (!r->bad_fills++)
				r->first_bad = at;
		}
		++r->fills;
		StrmdSndUnlock();
	}
}

TEST(unit_snd, cue_change_while_mixing)
{
	static struct range ranges[MAX_RANGES];
	static struct track_pcm tracks[MAX_IDS];
	struct mixer_race r;
	int32_t n = music_ranges(ranges, MAX_RANGES);
	int32_t ntracks = distinct_tracks(ranges, n, tracks, MAX_IDS);
	int32_t track = -1, changes = 0, before = DecodeOutOfRangeReads();
	int32_t offsets[MAX_RANGES][3];
	int32_t noff = 0;
	struct rsrc *sndq = NULL;

	for (int32_t t = 0, best = 0; t < ntracks; ++t) {
		int32_t count = 0;
		for (int32_t i = 0; i < n; ++i)
			count += ranges[i].track == tracks[t].id;
		if (tracks[t].pcm && count > best) {
			best = count;
			track = t;
		}
	}
	CHECK(track >= 0);
	if (track >= 0)
		sndq = AssetsGetElement(g_data_lib, "SndQ", tracks[track].id, false);
	CHECK(sndq != NULL);
	if (!sndq) {
		free_tracks(tracks, ntracks);
		return;
	}
	for (int32_t i = 0; i < n; ++i) {
		if (ranges[i].track != tracks[track].id)
			continue;
		offsets[noff][0] = cue_offset(sndq, ranges[i].start_cue);
		offsets[noff][1] = cue_offset(sndq, ranges[i].loop_cue);
		offsets[noff][2] = cue_offset(sndq, ranges[i].end_cue);
		if (offsets[noff][0] >= 0 && offsets[noff][0] < offsets[noff][2])
			++noff;
	}
	RsrcDestroy(&sndq);
	CHECK(noff > 1);

	memset(&r, 0, sizeof(r));
	r.upk = open_track(tracks[track].id);
	r.pcm = tracks[track].pcm;
	r.total = tracks[track].total;
	r.snd = WinMixedStreamedSndCreate(NULL, &r.upk->base, MIXER_PIECE);
	r.snd->base.looping = true;
	r.snd->play_ptr = r.snd->mem;

	struct test_thread *mixer = TestThreadStart(mixer_thread, &r);
	CHECK(mixer != NULL);
	uint32_t until = TimeTicks() + 3000;
	while (mixer && TimeTicks() < until) {
		const int32_t *o = offsets[changes % noff];
		SND_OBJ_OPS(&r.snd->base.base)->SetLoop(&r.snd->base.base, o[0], o[1], o[2], 0);
		++changes;
	}
	r.stop = true;
	TestThreadJoin(&mixer);
	TNote("track %d: %d cue changes, %d mixer reads", tracks[track].id, changes, r.fills);
	CHECK_MSG(r.bad_fills == 0, "%d of %d mixer reads differ from the whole decode (first at sample %d)", r.bad_fills,
		r.fills, r.first_bad);
	CHECK_EQ(DecodeOutOfRangeReads() - before, 0);
	ObjectDelete(r.snd);
	free_tracks(tracks, ntracks);
}

const struct test_reg TESTS_unit_snd[] = {
	TEST_ENTRY(unit_snd, cue_ranges_match_whole_decode),
	TEST_ENTRY(unit_snd, seek_to_every_key_frame),
	TEST_ENTRY(unit_snd, back_reference_past_buffer),
	TEST_ENTRY(unit_snd, cue_change_while_mixing),
	TEST_END
};

#endif

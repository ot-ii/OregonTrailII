#include "../freeguard.h"

#ifdef DIAG_BUILD

#include <stdint.h>
#include <string.h>

#include <windows.h>

#include "../crashlog.h"
#include "../memtrack.h"
#include "../../base/str.h"

#define LIVE 0x4C495645u
#define FREED 0x46524545u
#define QUARANTINE_BLOCKS 32768
#define QUARANTINE_BYTES ((size_t) 128 * 1024 * 1024)
#define DEPTH 24

struct header {
	uint32_t magic;
	uint32_t size;
	int32_t slot;
	int32_t site;
};

struct freed {
	struct header *h;
	uint32_t size;
	USHORT depth;
	void *stack[DEPTH];
};

static struct freed g_freed[QUARANTINE_BLOCKS];
static int32_t g_head, g_count;
static size_t g_bytes;
static SRWLOCK g_lock = SRWLOCK_INIT;
static volatile LONG g_reporting;
static volatile LONG g_foreign_reported;
static HANDLE g_heap;
static INIT_ONCE g_init_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK init_once(PINIT_ONCE once, PVOID param, PVOID *context)
{
	(void) once;
	(void) param;
	(void) context;

	g_heap = HeapCreate(0, 0, 0);

	return g_heap != NULL;
}

static void ensure_ready(void)
{
	if (!g_heap)
		InitOnceExecuteOnce(&g_init_once, init_once, NULL, NULL);
}

static void rvas(FILE *f, void *const *stack, int32_t depth)
{
	char *base = (char *) GetModuleHandleA(NULL);

	for (int32_t i = 0; i < depth; ++i)
		fprintf(f, "    rva %08lX\n", (unsigned long) ((char *) stack[i] - base));
}

static FILE *open_report(void)
{
	char path[MAX_PATH];
	char *dot;
	FILE *f;

	if (!GetModuleFileNameA(NULL, path, MAX_PATH))
		return NULL;
	path[MAX_PATH - 1] = 0;
	dot = strrchr(path, '.');
	if (dot && !strchr(dot, '\\'))
		*dot = 0;
	StrCat(path, "_freeguard.txt");

	if (fopen_s(&f, path, "a") != 0)
		return NULL;

	return f;
}

static void freed_again(struct header *h, void *p, const char *what, void *const *now, USHORT depth)
{
	FILE *f;

	if (InterlockedExchange(&g_reporting, 1))
		return;

	f = open_report();
	if (f) {
		fprintf(f, "%s of %p (%u bytes)\n  second call from:\n", what, p, h->size);
		rvas(f, now, depth);
		if (h->slot >= 0 && h->slot < QUARANTINE_BLOCKS && g_freed[h->slot].h == h) {
			fprintf(f, "  first free from:\n");
			rvas(f, g_freed[h->slot].stack, g_freed[h->slot].depth);
		}
		fclose(f);
	}

	RaiseException(0xE0FEEE01, EXCEPTION_NONCONTINUABLE, 0, NULL);
}

static void foreign(void *p, const char *what, void *const *now, USHORT depth)
{
	FILE *f;

	if (InterlockedExchange(&g_foreign_reported, 1))
		return;

	f = open_report();
	if (f) {
		fprintf(f, "%s of %p, which MemAlloc did not hand out (left alone), from:\n", what, p);
		rvas(f, now, depth);
		fclose(f);
	}
}

static void *alloc_block(size_t bytes, void *const *site)
{
	struct header *h;

	if (bytes > UINT32_MAX - sizeof(struct header)) {
		CrashLogAllocFailure(bytes);
		return NULL;
	}

	ensure_ready();
	h = g_heap ? HeapAlloc(g_heap, HEAP_ZERO_MEMORY, bytes + sizeof(struct header)) : NULL;
	if (!h) {
		CrashLogAllocFailure(bytes);
		return NULL;
	}

	h->magic = LIVE;
	h->size = (uint32_t) bytes;
	h->slot = -1;
	h->site = MemTrackAlloc(site[0], site[1], bytes);

	return h + 1;
}

static void release(struct header *h, void *const *stack, USHORT depth)
{
	struct freed *slot;

	MemTrackFree(h->site, h->size);
	memset(h + 1, 0xDD, h->size);
	h->magic = FREED;

	AcquireSRWLockExclusive(&g_lock);
	while (g_count && (g_count == QUARANTINE_BLOCKS || g_bytes + h->size > QUARANTINE_BYTES)) {
		struct freed *old = &g_freed[(g_head - g_count + QUARANTINE_BLOCKS) % QUARANTINE_BLOCKS];
		g_bytes -= old->size;
		--g_count;
		HeapFree(g_heap, 0, old->h);
		old->h = NULL;
	}
	slot = &g_freed[g_head];
	h->slot = g_head;
	slot->h = h;
	slot->size = h->size;
	slot->depth = depth;
	memcpy(slot->stack, stack, sizeof(slot->stack));
	g_head = (g_head + 1) % QUARANTINE_BLOCKS;
	++g_count;
	g_bytes += h->size;
	ReleaseSRWLockExclusive(&g_lock);
}

void *FreeGuardAlloc(size_t count, size_t size)
{
	void *site[2] = {NULL, NULL};

	if (size && count > SIZE_MAX / size) {
		CrashLogAllocFailure(SIZE_MAX);
		return NULL;
	}

	RtlCaptureStackBackTrace(2, 2, site, NULL);

	return alloc_block(count * size, site);
}

void *FreeGuardRealloc(void *mem, size_t size)
{
	void *stack[DEPTH] = {0};
	USHORT depth = RtlCaptureStackBackTrace(2, DEPTH, stack, NULL);
	struct header *h;
	void *out;

	if (!mem)
		return alloc_block(size, stack);

	h = (struct header *) mem - 1;
	if (h->magic == FREED) {
		freed_again(h, mem, "REALLOC OF A FREED BLOCK", stack, depth);
		return NULL;
	}
	if (h->magic != LIVE) {
		foreign(mem, "REALLOC", stack, depth);
		RaiseException(0xE0FEEE02, EXCEPTION_NONCONTINUABLE, 0, NULL);
		return NULL;
	}

	out = alloc_block(size, stack);
	if (!out)
		return NULL;
	memcpy(out, mem, h->size < size ? h->size : size);
	release(h, stack, depth);

	return out;
}

void FreeGuardFree(void *mem)
{
	void *stack[DEPTH] = {0};
	USHORT depth;
	struct header *h;

	if (!mem)
		return;

	depth = RtlCaptureStackBackTrace(2, DEPTH, stack, NULL);
	h = (struct header *) mem - 1;
	if (h->magic == FREED) {
		freed_again(h, mem, "DOUBLE FREE", stack, depth);
		return;
	}
	if (h->magic != LIVE) {
		foreign(mem, "FREE", stack, depth);
		return;
	}

	release(h, stack, depth);
}

void FreeGuardDescribe(FILE *f, const void *addr, const char *name)
{
	const char *a = (const char *) addr;

	for (int32_t i = 0; i < QUARANTINE_BLOCKS; ++i) {
		const struct freed *s = &g_freed[i];
		const char *start;

		if (!s->h)
			continue;
		start = (const char *) (s->h + 1);
		if (a >= (const char *) s->h && a < start + s->size) {
			fprintf(f, "%s = %p is inside a FREED block %p (%u bytes, offset %d), freed from:\n", name, addr,
				(const void *) start, s->size, (int) (a - start));
			rvas(f, s->stack, s->depth);
		}
	}
}

#endif

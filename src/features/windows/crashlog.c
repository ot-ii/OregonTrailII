//

#include "../crashlog.h"

#ifdef DIAG_BUILD

#include <rtcapi.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <intrin.h>
#include <windows.h>
#include <dbghelp.h>

#include "../freeguard.h"
#include "../../app/replay.h"
#include "../../base/str.h"

typedef BOOL (WINAPI *MiniDumpWriteDumpFn)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
	PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

static volatile LONG g_writing;
static volatile LONG g_alloc_failure_written;

static void build_path(char *out, size_t out_size, const char *dir, const char *ext, const SYSTEMTIME *now)
{
	char exe[MAX_PATH];
	char *dot;
	const char *name;

	if (!GetModuleFileNameA(NULL, exe, MAX_PATH))
		StrCopy(exe, "otii32.exe");
	exe[MAX_PATH - 1] = 0;
	dot = strrchr(exe, '.');
	if (dot && !strchr(dot, '\\'))
		*dot = 0;

	if (dir) {
		name = strrchr(exe, '\\');
		name = name ? name + 1 : exe;
		StrPrintfN(out, out_size, "%s\\%s_%04d%02d%02d_%02d%02d%02d%s", dir, name, now->wYear, now->wMonth,
			now->wDay, now->wHour, now->wMinute, now->wSecond, ext);

	} else {
		StrPrintfN(out, out_size, "%s_%04d%02d%02d_%02d%02d%02d%s", exe, now->wYear, now->wMonth, now->wDay,
			now->wHour, now->wMinute, now->wSecond, ext);
	}
}

static bool local_dump_dir(char *out, size_t out_size)
{
	char base[MAX_PATH];
	DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", base, MAX_PATH);

	if (!n || n >= MAX_PATH)
		return false;
	StrPrintfN(out, out_size, "%s\\CrashDumps", base);
	CreateDirectoryA(out, NULL);

	return GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES;
}

static HANDLE create_dump_file(char *dump_path, size_t dump_size, char *text_path, size_t text_size)
{
	SYSTEMTIME now;
	char dir[MAX_PATH];
	HANDLE file;

	GetLocalTime(&now);
	build_path(dump_path, dump_size, NULL, ".dmp", &now);
	build_path(text_path, text_size, NULL, ".txt", &now);
	file = CreateFileA(dump_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file != INVALID_HANDLE_VALUE || !local_dump_dir(dir, sizeof(dir)))
		return file;

	build_path(dump_path, dump_size, dir, ".dmp", &now);
	build_path(text_path, text_size, dir, ".txt", &now);

	return CreateFileA(dump_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

static void write_summary(const char *path, const char *what, EXCEPTION_POINTERS *info, const char *dump_path)
{
	FILE *f;
	HMODULE base;

	if (fopen_s(&f, path, "w") != 0)
		return;
	base = GetModuleHandleA(NULL);
	fprintf(f, "%s\n", what);
	fprintf(f, "module base %p\n", (void *) base);
	if (dump_path)
		fprintf(f, "minidump    %s\n", dump_path);

	if (info && info->ExceptionRecord) {
		void *addr = info->ExceptionRecord->ExceptionAddress;

		fprintf(f, "exception   %08lX at %p (rva %08lX -- look this up in the build's .map)\n",
			(unsigned long) info->ExceptionRecord->ExceptionCode, addr,
			(unsigned long) ((char *) addr - (char *) base));
		if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
				info->ExceptionRecord->NumberParameters >= 2)
			fprintf(f, "            %s at %p\n", info->ExceptionRecord->ExceptionInformation[0] ? "write" : "read",
				(void *) info->ExceptionRecord->ExceptionInformation[1]);
	}

	if (info && info->ContextRecord) {
		CONTEXT *c = info->ContextRecord;

#ifdef _WIN64
		fprintf(f, "rip=%016llX rsp=%016llX rbp=%016llX\n", c->Rip, c->Rsp, c->Rbp);
		fprintf(f, "rax=%016llX rbx=%016llX rcx=%016llX rdx=%016llX\n", c->Rax, c->Rbx, c->Rcx, c->Rdx);
		fprintf(f, "rsi=%016llX rdi=%016llX r8=%016llX r9=%016llX\n", c->Rsi, c->Rdi, c->R8, c->R9);
#else
		fprintf(f, "eip=%08lX esp=%08lX ebp=%08lX\n", c->Eip, c->Esp, c->Ebp);
		fprintf(f, "eax=%08lX ebx=%08lX ecx=%08lX edx=%08lX esi=%08lX edi=%08lX\n", c->Eax, c->Ebx, c->Ecx, c->Edx,
			c->Esi, c->Edi);

		DWORD *fp = (DWORD *) (DWORD_PTR) c->Ebp;

		for (int32_t depth = 0; depth < 32 && fp; ++depth) {
			DWORD ret;

			if (IsBadReadPtr(fp, 8))
				break;
			ret = fp[1];
			if (!ret)
				break;
			fprintf(f, "  frame %2d: ret %08lX  rva %08lX\n", depth, (unsigned long) ret,
				(unsigned long) (ret - (DWORD) (DWORD_PTR) base));
			if (fp[0] <= (DWORD) (DWORD_PTR) fp)
				break;
			fp = (DWORD *) (DWORD_PTR) fp[0];
		}
#endif
	}

	fclose(f);
}

static void write_freed_check(const char *path, EXCEPTION_POINTERS *info)
{
	FILE *f;
	const CONTEXT *c;

	if (!info || !info->ContextRecord || fopen_s(&f, path, "a") != 0)
		return;

	c = info->ContextRecord;
	fprintf(f, "freed-block check (0xDDDDDDDD in a register means a read from a freed block):\n");
#ifdef _WIN64
	FreeGuardDescribe(f, (void *) c->Rax, "rax");
	FreeGuardDescribe(f, (void *) c->Rbx, "rbx");
	FreeGuardDescribe(f, (void *) c->Rcx, "rcx");
	FreeGuardDescribe(f, (void *) c->Rdx, "rdx");
	FreeGuardDescribe(f, (void *) c->Rsi, "rsi");
	FreeGuardDescribe(f, (void *) c->Rdi, "rdi");
#else
	FreeGuardDescribe(f, (void *) (DWORD_PTR) c->Eax, "eax");
	FreeGuardDescribe(f, (void *) (DWORD_PTR) c->Ebx, "ebx");
	FreeGuardDescribe(f, (void *) (DWORD_PTR) c->Ecx, "ecx");
	FreeGuardDescribe(f, (void *) (DWORD_PTR) c->Edx, "edx");
	FreeGuardDescribe(f, (void *) (DWORD_PTR) c->Esi, "esi");
	FreeGuardDescribe(f, (void *) (DWORD_PTR) c->Edi, "edi");
#endif
	if (info->ExceptionRecord && info->ExceptionRecord->NumberParameters >= 2)
		FreeGuardDescribe(f, (void *) info->ExceptionRecord->ExceptionInformation[1], "fault address");
	fclose(f);
}

static void write_dump(const char *what, EXCEPTION_POINTERS *info, volatile LONG *once)
{
	char dump_path[MAX_PATH];
	char text_path[MAX_PATH];
	HMODULE dbghelp;
	MiniDumpWriteDumpFn write_fn;
	BOOL wrote = FALSE;
	HANDLE file;

	if (InterlockedExchange(once, 1))
		return;

	file = create_dump_file(dump_path, sizeof(dump_path), text_path, sizeof(text_path));

	dbghelp = LoadLibraryA("dbghelp.dll");
	write_fn = dbghelp ? (MiniDumpWriteDumpFn) (void (*)(void)) GetProcAddress(dbghelp, "MiniDumpWriteDump") : NULL;

	if (file != INVALID_HANDLE_VALUE) {
		if (write_fn) {
			MINIDUMP_EXCEPTION_INFORMATION mei;
			MINIDUMP_EXCEPTION_INFORMATION *meip = NULL;

			if (info) {
				mei.ThreadId = GetCurrentThreadId();
				mei.ExceptionPointers = info;
				mei.ClientPointers = FALSE;
				meip = &mei;
			}
			wrote = write_fn(GetCurrentProcess(), GetCurrentProcessId(), file,
				(MINIDUMP_TYPE) (MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory |
					MiniDumpWithHandleData | MiniDumpWithFullMemory), meip, NULL, NULL);
		}
		CloseHandle(file);
		if (!wrote)
			DeleteFileA(dump_path);
	}

	write_summary(text_path, what, info, wrote ? dump_path : NULL);
	RpFlush();
	write_freed_check(text_path, info);
}

static LONG WINAPI crash_filter(EXCEPTION_POINTERS *info)
{
	write_dump("unhandled exception", info, &g_writing);

	return EXCEPTION_EXECUTE_HANDLER;
}

static void __cdecl on_invalid_parameter(const wchar_t *expr, const wchar_t *func, const wchar_t *file, unsigned int line,
	uintptr_t reserved)
{
	(void) expr;
	(void) func;
	(void) file;
	(void) line;
	(void) reserved;

	write_dump("CRT invalid parameter", NULL, &g_writing);
	_exit(5);
}

static void __cdecl on_abort(int sig)
{
	(void) sig;

	write_dump("abort()", NULL, &g_writing);
	_exit(6);
}

void CrashLogAllocFailure(size_t bytes)
{
	char what[64];

	StrPrintf(what, "MemAlloc failed for %zu bytes", bytes);
	write_dump(what, NULL, &g_alloc_failure_written);
}

static int __cdecl on_rtc_failure(int type, const wchar_t *file, int line, const wchar_t *module,
	const wchar_t *format, ...)
{
	char what[512];
	wchar_t msg[256];
	va_list args;

	va_start(args, format);
	vswprintf(msg, 256, format ? format : L"", args);
	va_end(args);
	StrPrintf(what, "RTC failure %d at %ls(%d) in %ls: %ls  [caller %p]", type, file ? file : L"?", line,
		module ? module : L"?", msg, _ReturnAddress());
	write_dump(what, NULL, &g_writing);
	_exit(7);
}

void CrashLogInstall(void)
{
	_RTC_SetErrorFuncW(on_rtc_failure);
	SetUnhandledExceptionFilter(crash_filter);
	_set_invalid_parameter_handler(on_invalid_parameter);
	signal(SIGABRT, on_abort);
}

#endif

#include <stddef.h>

#include <windows.h>

#include "../../dialogs/alert.h"
#include "../app.h"
#include "../../media/cinepakframe.h"
#include "../../features/clicontrol.h"
#include "../../features/crashlog.h"
#include "../appvideo.h"
#include "../../base/cstr.h"
#include "../../base/log.h"
#include "../registrar.h"
#include "../replay.h"
#include "../../base/rng.h"
#include "../../ui/scaler.h"
#include "../../media/strmdsnd.h"

static void fatal_alert(const char *text, void *opaque)
{
	(void) opaque;

	if (g_alert_handler) {
		struct cstr *message = CStrCreateFrom(text);
		AlertShow(g_alert_handler, 1, message, 0, 1);
		CStrDestroy(&message);
	}
}

static void trace_rand(int32_t lo, int32_t hi, int32_t count, int32_t result)
{
	RpTraceRand(lo, hi, count, result);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	(void) lpCmdLine;
	(void) nShowCmd;

	g_registrar_singleton = RegistrarCreate();
	StrmdSndInstallHooks();
	LogSetFatal(fatal_alert, NULL);

	RngSetTimeSource(RpTime);
	RngSetTrace(trace_rand);

	ScalerProcessInit();
	RpSetScalerHooks(ScalerPresentAll, ScalerForce);
#ifdef CLI_CONTROL
	CliControlStart();
#endif

	RpMode();

#ifdef DIAG_BUILD
	CrashLogInstall();
#endif

#ifdef _WIN64
	CinepakInstall();
#endif

	g_hinstance = hInstance;

	if (!hPrevInstance)
		RegisterWindowClasses();

	struct app *app = OtiiAppCreate();
	APP_OPS(app)->RunUntilQuit(app);
	ObjectDelete(app);

	return 0;
}

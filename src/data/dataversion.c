#include "dataversion.h"

#include <stddef.h>
#include <stdint.h>

#include "assets.h"
#include "gfxdev.h"
#include "rsrc.h"

static int32_t g_data_version = -1;

static bool has_element(struct assets *lib, const char *type, int32_t id)
{
	struct rsrc *rsrc;

	if (!lib)
		return false;
	rsrc = AssetsGetElement(lib, type, id, false);
	if (!rsrc)
		return false;
	RsrcDestroy(&rsrc);
	return true;
}

bool DataIs13(void)
{
	if (g_data_version < 0) {
		if (!g_data_lib || !g_text_lib)
			return false;

		// Detect if there are the graphics for the 3 buttons at the bottom
		g_data_version = has_element(g_data_lib, "CBMP", 612) && has_element(g_data_lib, "CBMP", 22201) &&
			has_element(g_data_lib, "CBMP", 22202) && has_element(g_data_lib, "CBMP", 22203) &&
			has_element(g_text_lib, "STRLST", 2208) && has_element(g_text_lib, "STRLST", 2214);
	}
	return g_data_version != 0;
}

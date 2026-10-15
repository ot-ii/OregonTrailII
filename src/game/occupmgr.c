#include "occupmgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../data/gfxdev.h"
#include "../base/log.h"
#include "../base/memory.h"
#include "../data/rsrc.h"
#include "../data/strrsrc.h"

static void parse_table(struct occup_mgr *self, struct rsrc *r)
{
	const uint8_t *data = RsrcData(r);
	int32_t size = RsrcSize(r);
	int32_t count = size >= 2 ? ReadLe16(data) : 0;

	if (count < 0)
		count = 0;

	if (count > (size - 2) / 8)
		count = (size - 2) / 8;

	self->table = MemAlloc((size_t) count, sizeof(struct occup_row));
	self->table_count = count;

	for (int32_t i = 0; i < count; i++) {
		const uint8_t *row = data + 2 + 8 * i;

		self->table[i].starting_cash = ReadLe16(row);
		self->table[i].score_mult_x10 = ReadLe16(row + 2);

		for (int32_t k = 0; k < 4; k++)
			self->table[i].skill[k] = (int8_t) row[4 + k];
	}
}

struct occup_mgr *OccupMgrCreate(void)
{
	struct occup_mgr *self = MemAlloc(1, sizeof(struct occup_mgr));

	self->names = NULL;

	self->skill_names = StrRsrcLoad(g_text_lib, 2201, true);
	self->skill_costs = StrRsrcLoad(g_text_lib, 2205, true);

	struct rsrc *r = AssetsGetElement(g_data_lib, "OCCUP", 1, true);

	if (r && RsrcStorage(r) == RSRC_TEXT) {
		LogError(LOG_ERROR, "Attempt load a raw resource");

	} else if (r) {
		RsrcLoadAndUnpack(r);
		parse_table(self, r);
		self->names = StrRsrcLoad(g_text_lib, 2200, true);
	}

	RsrcDestroy(&r);

	return self;
}

void OccupMgrDestroy(struct occup_mgr **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct occup_mgr *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx->table);
	StrRsrcDestroy(&ctx->names);
	StrRsrcDestroy(&ctx->skill_names);
	StrRsrcDestroy(&ctx->skill_costs);

	MemFree(ctx);
}

static const struct occup_row *get_row(struct occup_mgr *self, int32_t row)
{
	if (!self->table || row < 0 || row >= self->table_count)
		return NULL;

	return &self->table[row];
}

int32_t OccupMgrGetStartingCash(struct occup_mgr *self, int32_t row)
{
	const struct occup_row *r = get_row(self, row);

	return r ? r->starting_cash : 0;
}

int32_t OccupMgrGetScoreMultX10(struct occup_mgr *self, int32_t row)
{
	const struct occup_row *r = get_row(self, row);

	return r ? r->score_mult_x10 : 0;
}

bool OccupMgrHasSkill(struct occup_mgr *self, int32_t row, int32_t value)
{
	bool found = false;
	const struct occup_row *r = get_row(self, row);

	if (!r)
		return false;

	for (int32_t slot = 0; slot < 4; slot++) {
		if (r->skill[slot] == value)
			found = true;
	}

	return found;
}

int32_t OccupMgrGetSkillCost(struct occup_mgr *self, int32_t line)
{
	int32_t cost = 0;
	const char *text = StrRsrcGetAt(self->skill_costs, line);

	if (text)
		cost = (int16_t) atoi(text);

	return cost;
}

void OccupMgrBuildTable(struct occup_mgr *self, struct rsrc *raw)
{
	char name_buf[32];
	char score_mult_buf[16];
	char skill_buf[640];
	char cash_buf[32];

	StrRsrcDestroy(&self->names);
	self->names = StrRsrcCreate();

	int32_t row_cap = 25;
	char *text = (char *) RsrcData(raw);
	struct occup_row *rows = MemAlloc((size_t) row_cap, sizeof(struct occup_row));
	int32_t count = 0;
	bool done = false;
	bool started = false;

	while (!done) {
		char *scan;

		for (scan = text; *scan != 13 && *scan; ++scan)
			continue;

		if (!*scan)
			done = true;

		*scan = 9;

		if (started) {
			if (row_cap == count) {
				++row_cap;
				rows = MemRealloc(rows, (size_t) row_cap, sizeof(struct occup_row));
			}

			sscanf(text, "%31[^\t] $%31[^\t] %511[^\t] %127[^\t] x%15[^\t]", name_buf, cash_buf, &skill_buf[128],
				skill_buf, score_mult_buf);
			StrRsrcAppend(self->names, name_buf);
			rows[count].starting_cash = (int16_t) atoi(cash_buf);
			rows[count].score_mult_x10 = (int16_t) (atof(score_mult_buf) * 10.0);

			char *skill_ptr = skill_buf;

			for (int32_t k = 0; k <= 3; ++k) {
				if (!skill_ptr) {
					rows[count].skill[k] = -1;

				} else {
					char *comma = strchr(skill_ptr, ',');

					if (comma)
						*comma = 0;

					rows[count].skill[k] = (int8_t) StrRsrcIndexOf(self->skill_names, skill_ptr);
					skill_ptr = comma ? comma + 2 : NULL;
				}
			}

			++count;

		} else {
			started = true;
		}

		text = scan + 1;
	}

	MemFree(self->table);
	self->table = rows;
	self->table_count = count;
}

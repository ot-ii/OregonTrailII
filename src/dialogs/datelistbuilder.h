#pragma once

#include <stdint.h>

struct cstr;
struct strrsrc;

struct option_entry {
	const char *name;
	int32_t min_ctx;
	int32_t max_ctx;
	int32_t code8;
	int32_t code10;
};

extern struct option_entry g_destination_options[8];

struct date_list_builder;

struct date_list_builder *DateListBuilderCreate(void);
void DateListBuilderDestroy(struct date_list_builder **ctx_mod);

struct strrsrc *DateListBuilderBuildStartTownList(struct date_list_builder *ctx, int32_t ctx_year,
	const struct cstr *current, int32_t fallback, int32_t *sel_out);
struct strrsrc *DateListBuilderBuildDestinationList(struct date_list_builder *ctx, int32_t ctx_year,
	const struct cstr *current, int32_t fallback, int32_t *sel_out);
int32_t DateListBuilderLookupStartTown(struct date_list_builder *ctx, const struct cstr *name);
int32_t DateListBuilderLookupDestination(struct date_list_builder *ctx, const struct cstr *name);
struct strrsrc *DateListBuilderBuildFilteredComboList(struct date_list_builder *ctx, int32_t ctx_year,
	const struct cstr *current, int32_t fallback, int32_t *sel_out, const struct option_entry *table);
int32_t DateListBuilderTableLookupField8(struct date_list_builder *ctx, const struct cstr *name,
	const struct option_entry *table);
int32_t DateListBuilderTableLookupField10(struct date_list_builder *ctx, const struct cstr *name,
	const struct option_entry *table);

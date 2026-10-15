#pragma once

#include <stdint.h>

#include "../data/docfile.h"
#include "../data/saveformat.h"

struct path;

#define GAMEDOC_TYPE 0x4F543221
#define GAMEDOC_VER SAVE_FORMAT_2_0
#define GAMEDOC_VER_MIN SAVE_FORMAT_1_2

struct game_doc {
	struct doc_file base;
};

extern const struct doc_file_ops GAME_DOC_OPS_TABLE;

void GameDocInit(struct game_doc *self, int32_t command);
void GameDocInitPath(struct game_doc *self, struct path *source);
struct game_doc *GameDocCreate(int32_t command);
struct game_doc *GameDocCreatePath(struct path *source);
void GameDocDestroy(struct game_doc **ctx_mod);
void game_doc_destroy(struct object *self);

int32_t game_doc_open_read(struct doc_file *self);
int32_t game_doc_open_default(struct doc_file *self);

int32_t GameDocWriteCopy(struct path *path);

#ifdef OTII_TEST
int32_t GameDocReadIntoGame(struct path *path);
#endif

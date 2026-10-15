#include "gamedoc.h"

#include <stddef.h>

#include "../base/byteorder.h"
#include "../base/cstr.h"
#include "../data/datafile.h"
#include "../base/file.h"
#include "../base/memory.h"
#include "../app/otiiapp.h"
#include "../base/path.h"

const struct doc_file_ops GAME_DOC_OPS_TABLE = {
	{game_doc_destroy},
	game_doc_open_read,
	doc_file_open_save,
	game_doc_open_default,
};

void GameDocInit(struct game_doc *self, int32_t command)
{
	DocFileInit(&self->base);
	self->base.base.ops = &GAME_DOC_OPS_TABLE.base;

	struct otii_app *app = (struct otii_app *) g_app;
	OtiiAppNewGameSetup(app, command);
}

void GameDocInitPath(struct game_doc *self, struct path *source)
{
	DocFileInitPath(&self->base, source);
	self->base.base.ops = &GAME_DOC_OPS_TABLE.base;

	DOC_FILE_OPS(self)->OpenRead(&self->base);
}

struct game_doc *GameDocCreate(int32_t command)
{
	struct game_doc *self = MemAlloc(1, sizeof(struct game_doc));
	GameDocInit(self, command);

	return self;
}

struct game_doc *GameDocCreatePath(struct path *source)
{
	struct game_doc *self = MemAlloc(1, sizeof(struct game_doc));
	GameDocInitPath(self, source);

	return self;
}

void GameDocDestroy(struct game_doc **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void game_doc_destroy(struct object *self)
{
	self->ops = &DOC_FILE_OPS_TABLE.base;
	doc_file_destroy(self);
}

static bool gamedoc_blocks_intact(struct doc_file *doc, int32_t version)
{
	int64_t size, pos = 10;
	int32_t len, count = 0, first_len = -1, settings_len = 0, game_len = 0;
	struct file *stream;
	uint8_t len_bytes[4];

	if (!doc->stream || !doc->path || !doc->path->path)
		return false;
	stream = FileOpen(CStrGet(doc->path->path), FILE_READ);
	if (!stream)
		return false;
	size = FileSize(stream);
	while (pos < size) {
		len = 0;
		if (pos + 4 > size || !FileSeek(stream, pos) || FileRead(stream, len_bytes, 4) != 4) {
			FileClose(&stream);
			return false;
		}
		len = ReadLe32(len_bytes);
		if (len < 0 || len > size - pos - 4) {
			FileClose(&stream);
			return false;
		}
		if (count == 0)
			first_len = len;
		else if (count == 1)
			settings_len = len;
		else if (count == 2)
			game_len = len;
		pos += 4 + (int64_t) len;
		++count;
	}
	FileClose(&stream);
	DataFileSeek(doc->stream, 10);
	return pos == size && count >= 8 && first_len == 8 && settings_len > 0 &&
		game_len >= ((uint32_t) (version & 0xFFFF) >= SAVE_FORMAT_2_0 ? 84 : 480);
}

int32_t game_doc_open_read(struct doc_file *self)
{
	struct otii_app *app = (struct otii_app *) g_app;

	if (!doc_file_open_read(self)) {
		if (!DocFileReadHeader(self, GAMEDOC_TYPE, GAMEDOC_VER_MIN, GAMEDOC_VER)) {
			if (gamedoc_blocks_intact(self, self->version))
				OtiiAppLoadFromDataFile(app, self);
			else
				DocFileSetStatus(self, 3);
		}
	}
	DataFileClose(&self->stream);
	return self->status;
}

int32_t game_doc_open_default(struct doc_file *self)
{
	struct otii_app *doc = (struct otii_app *) g_app;

	if (!doc_file_open_save(self, "Saved Games", "*.ot2")) {
		if (!DocFileWriteHeader(self, GAMEDOC_TYPE, GAMEDOC_VER)) {
			OtiiAppSaveToDataFile(doc, self);
			if (!DocFileGetStatus(self))
				DocFileSetDirtyV(self, false);
		}
	}
	DataFileClose(&self->stream);
	return self->status;
}

int32_t GameDocWriteCopy(struct path *path)
{
	struct otii_app *app = (struct otii_app *) g_app;
	struct doc_file file;
	int32_t status;

	DocFileInitPath(&file, path);
	if (!DOC_FILE_OPS(&file)->OpenSave(&file, NULL, NULL)) {
		if (!DocFileWriteHeader(&file, GAMEDOC_TYPE, GAMEDOC_VER))
			OtiiAppSaveToDataFile(app, &file);
	}
	status = DocFileGetStatus(&file);
	DataFileClose(&file.stream);
	OBJECT_OPS(&file)->destroy((struct object *) &file);
	return status;
}

#ifdef OTII_TEST
int32_t GameDocReadIntoGame(struct path *path)
{
	struct otii_app *app = (struct otii_app *) g_app;
	struct doc_file file;
	int32_t status;

	DocFileInitPath(&file, path);
	if (!doc_file_open_read(&file)) {
		if (!DocFileReadHeader(&file, GAMEDOC_TYPE, GAMEDOC_VER_MIN, GAMEDOC_VER)) {
			if (gamedoc_blocks_intact(&file, file.version))
				OtiiAppLoadFromDataFile(app, &file);
			else
				DocFileSetStatus(&file, 3);
		}
	}
	status = DocFileGetStatus(&file);
	DataFileClose(&file.stream);
	OBJECT_OPS(&file)->destroy((struct object *) &file);
	return status;
}
#endif

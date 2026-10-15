#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DATAFILE_OK          0
#define DATAFILE_DISK_FULL   2
#define DATAFILE_IO_ERROR    3
#define DATAFILE_NOT_MECC    4
#define DATAFILE_WRONG_TYPE  5
#define DATAFILE_BAD_VERSION 6

struct datafile;

struct datafile *DataFileOpenRead(const char *path);
struct datafile *DataFileOpenWrite(const char *path);
void DataFileClose(struct datafile **ctx_mod);

int32_t DataFileStatus(const struct datafile *ctx);
int32_t DataFileVersion(const struct datafile *ctx);

int32_t DataFileReadHeader(struct datafile *ctx, uint32_t type, uint32_t min_version, uint32_t max_version);
int32_t DataFileWriteHeader(struct datafile *ctx, uint32_t type, int32_t version);

uint8_t *DataFileReadBlock(struct datafile *ctx, int32_t *size);
int32_t DataFileWriteBlock(struct datafile *ctx, const void *buf, int32_t size);

int32_t DataFileReadBytes(struct datafile *ctx, void *buf, int32_t size);
int32_t DataFileWriteBytes(struct datafile *ctx, const void *buf, int32_t size);
bool DataFileSeek(struct datafile *ctx, int64_t pos);
int64_t DataFileTell(struct datafile *ctx);

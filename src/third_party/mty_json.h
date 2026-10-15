// libmatoya's JSON and hash table (github.com/snowcone-ltd/libmatoya, MIT, Copyright (c) Snowcone Ltd.; see
// libmatoya.LICENSE): the declarations from libmatoya's src\matoya.h that mty_json.c and mty_hash.c need, as they
// are upstream, and nothing else of matoya.h.
//
// Local changes:
//   - MTY_JSONReadFile and MTY_JSONWriteFile are left out (the port does not use them, and they would need
//     libmatoya's file functions).
//   - MTY_EXPORT is dropped (the code is linked in, not a DLL).
//   - The few libmatoya helpers the two files call (MTY_Alloc, MTY_Realloc, MTY_Free, MTY_Strdup, MTY_Log,
//     MTY_DJB2) are declared at the end; mty.c provides them on top of the port's memory and log modules.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


//- #module JSON
//- #mbrief JSON parsing and construction.

/// @brief JSON types.
typedef enum {
	MTY_JSON_NULL    = 0, ///< JSON `null` type.
	MTY_JSON_BOOL    = 1, ///< JSON boolean.
	MTY_JSON_NUMBER  = 2, ///< JSON number (double).
	MTY_JSON_STRING  = 3, ///< JSON string.
	MTY_JSON_ARRAY   = 4, ///< JSON array.
	MTY_JSON_OBJECT  = 5, ///< JSON object.
	MTY_JSON_MAKE_32 = INT32_MAX,
} MTY_JSONType;

typedef struct MTY_JSON MTY_JSON;

/// @brief Parse a string into an MTY_JSON item.
/// @param input Serialized JSON string.
/// @returns On failure, NULL is returned.\n\n
///   The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_JSON *
MTY_JSONParse(const char *input);

/// @brief Deep copy an MTY_JSON item.
/// @param json The MTY_JSON item to duplicate.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_JSON *
MTY_JSONDuplicate(const MTY_JSON *json);

/// @brief Get the MTY_JSONType of an MTY_JSON item.
/// @param json An MTY_JSON item.
MTY_JSONType
MTY_JSONGetType(const MTY_JSON *json);

/// @brief Destroy an MTY_JSON item.
/// @param json Passed by reference and set to NULL after being destroyed.\n\n
///   This function destroys all children of the item, meaning only the root item
///   in the hierarchy should ever be destroyed.
void
MTY_JSONDestroy(MTY_JSON **json);

/// @brief Serialize an MTY_JSON item into a string.
/// @param json An MTY_JSON item to serialize.
/// @returns The returned string must be destroyed with MTY_Free.
char *
MTY_JSONSerialize(const MTY_JSON *json);

/// @brief Create a new MTY_JSON null item.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_JSON *
MTY_JSONNullCreate(void);

/// @brief Create a new MTY_JSON boolean.
/// @param value The initialization value.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_JSON *
MTY_JSONBoolCreate(bool value);

/// @brief Get the bool value from an MTY_JSON boolean.
/// @param json An MTY_JSON boolean.
/// @param value The bool value from `json`.
/// @returns Returns true on success, false if the type of `json` is not MTY_JSON_BOOL.
bool
MTY_JSONBool(const MTY_JSON *json, bool *value);

/// @brief Create a new MTY_JSON number.
/// @param value The initialization value.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_JSON *
MTY_JSONNumberCreate(double value);

/// @brief Create a new MTY_JSON number, internally represented as a 32-bit integer.
/// @param value The initialization value.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_JSON *
MTY_JSONIntCreate(int32_t value);

/// @brief Get the double value from an MTY_JSON number.
/// @param json An MTY_JSON number.
/// @param value The double value from `json`.
/// @returns Returns true on success, false if the type of `json` is not MTY_JSON_NUMBER.
bool
MTY_JSONNumber(const MTY_JSON *json, double *value);

/// @brief Get the int8_t value from an MTY_JSON number.
/// @param json An MTY_JSON number.
/// @param value The int8_t value from `json`.
/// @returns Returns true on success, false if the type of `json` is not MTY_JSON_NUMBER.
bool
MTY_JSONInt8(const MTY_JSON *json, int8_t *value);

/// @brief Get the int16_t value from an MTY_JSON number.
/// @param json An MTY_JSON number.
/// @param value The int16_t value from `json`.
/// @returns Returns true on success, false if the type of `json` is not MTY_JSON_NUMBER.
bool
MTY_JSONInt16(const MTY_JSON *json, int16_t *value);

/// @brief Get the int32_t value from an MTY_JSON number.
/// @param json An MTY_JSON number.
/// @param value The int32_t value from `json`.
/// @returns Returns true on success, false if the type of `json` is not MTY_JSON_NUMBER.
bool
MTY_JSONInt32(const MTY_JSON *json, int32_t *value);

/// @brief Get the float value from an MTY_JSON number.
/// @param json An MTY_JSON number.
/// @param value The float value from `json`.
/// @returns Returns true on success, false if the type of `json` is not MTY_JSON_NUMBER.
bool
MTY_JSONFloat(const MTY_JSON *json, float *value);

/// @brief Create a new MTY_JSON string.
/// @param value Initialization UTF-8 string.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_JSON *
MTY_JSONStringCreate(const char *value);

/// @brief Get the string value from an MTY_JSON string.
/// @param json An MTY_JSON string.
/// @param value Filled with the string value from `json`.
/// @param size Size in bytes of `value`.
/// @returns Returns true on success, false if the type of `json` is not MTY_JSON_STRING or
///   the string value from `json` could not fit in `value`.
bool
MTY_JSONString(const MTY_JSON *json, char *value, size_t size);

/// @brief Get the internal string pointer from an MTY_JSON string.
/// @param json An MTY_JSON string.
/// @returns This reference is valid only as long as the `json` item is also valid.\n\n
///   If `json` is not an MTY_JSON string, NULL is returned.
const char *
MTY_JSONStringPtr(const MTY_JSON *json);

/// @brief Create a new MTY_JSON array.
/// @param len The number of elements allocated in the array.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_JSON *
MTY_JSONArrayCreate(uint32_t len);

/// @brief Get the number of items in an MTY_JSON array.
/// @param json An MTY_JSON item to query.
uint32_t
MTY_JSONArrayGetLength(const MTY_JSON *json);

/// @brief Get an item from an MTY_JSON array.
/// @param json An MTY_JSON array.
/// @param index Index to lookup.
/// @returns If the `index` exists, the item at that index is returned. This reference
///   is valid only as long as the `json` item is also valid.\n\n
///   If the `index` does not exist, NULL is returned.
const MTY_JSON *
MTY_JSONArrayGetItem(const MTY_JSON *json, uint32_t index);

/// @brief Set an item in an MTY_JSON array.
/// @param json An MTY_JSON array.
/// @param index The array index where `value` will be stored.
/// @param value Item to set at `index`.
/// @returns Returns true if `value` was set successfully, otherwise false.
bool
MTY_JSONArraySetItem(MTY_JSON *json, uint32_t index, MTY_JSON *value);

/// @brief Create a new MTY_JSON object.
/// @returns The returned MTY_JSON item should be destroyed with MTY_JSONDestroy if it
///   remains the root item in the hierarchy.
MTY_JSON *
MTY_JSONObjCreate(void);

/// @brief Iterate through all keys in an MTY_JSON object.
/// @param json An MTY_JSON object.
/// @param iter Iterator that keeps track of the position in the object. Set this to
///   0 before the fist call to this function.
/// @param key Reference to the next key in the object.
/// @returns Returns true if there are more keys available, otherwise false.
bool
MTY_JSONObjGetNextKey(const MTY_JSON *json, uint64_t *iter, const char **key);

/// @brief Get an item from an MTY_JSON object.
/// @param json An MTY_JSON object.
/// @param key Key to lookup.
/// @returns If the `key` exists, the associated item is returned. This reference is
///   valid only as long as the `json` item is also valid.\n\n
///   If the `key` does not exist, NULL is returned.
const MTY_JSON *
MTY_JSONObjGetItem(const MTY_JSON *json, const char *key);

/// @brief Set an item in an MTY_JSON object.
/// @details This function will replace an existing key with the same name.
/// @param json An MTY_JSON object.
/// @param key Key to set.
/// @param value Value associated with `key`.
/// @returns Returns true if `value` was set successfully, otherwise false.
bool
MTY_JSONObjSetItem(MTY_JSON *json, const char *key, MTY_JSON *value);

#define MTY_JSONObjGetBool(json, key, val) \
	MTY_JSONBool(MTY_JSONObjGetItem(json, key), val)

#define MTY_JSONObjGetInt8(json, key, val) \
	MTY_JSONInt8(MTY_JSONObjGetItem(json, key), val)

#define MTY_JSONObjGetInt16(json, key, val) \
	MTY_JSONInt16(MTY_JSONObjGetItem(json, key), val)

#define MTY_JSONObjGetInt(json, key, val) \
	MTY_JSONInt32(MTY_JSONObjGetItem(json, key), val)

#define MTY_JSONObjGetFloat(json, key, val) \
	MTY_JSONFloat(MTY_JSONObjGetItem(json, key), val)

#define MTY_JSONObjGetString(json, key, val, size) \
	MTY_JSONString(MTY_JSONObjGetItem(json, key), val, size)

#define MTY_JSONObjGetStringPtr(json, key) \
	MTY_JSONStringPtr(MTY_JSONObjGetItem(json, key))

#define MTY_JSONObjSetBool(json, key, val) \
	MTY_JSONObjSetItem(json, key, MTY_JSONBoolCreate(val))

#define MTY_JSONObjSetNumber(json, key, val) \
	MTY_JSONObjSetItem(json, key, MTY_JSONNumberCreate(val))

#define MTY_JSONObjSetInt(json, key, val) \
	MTY_JSONObjSetItem(json, key, MTY_JSONIntCreate(val))

#define MTY_JSONObjSetString(json, key, val) \
	MTY_JSONObjSetItem(json, key, MTY_JSONStringCreate(val))

#define MTY_JSONArraySetString(json, index, val) \
	MTY_JSONArraySetItem(json, index, MTY_JSONStringCreate(val))


//- #module Hash
//- #mbrief Simple hash map.

typedef struct MTY_Hash MTY_Hash;

/// @brief Function called on each item in an MTY_Hash when it is destroyed.
/// @param ptr Pointer set via MTY_HashSet et al.
typedef void (*MTY_FreeFunc)(void *ptr);

/// @brief Create an MTY_Hash for key/value lookup.
/// @param numBuckets Number of buckets, 0 for the default.
/// @returns The returned MTY_Hash must be destroyed with MTY_HashDestroy.
MTY_Hash *
MTY_HashCreate(uint32_t numBuckets);

/// @brief Destroy an MTY_Hash.
/// @param hash Passed by reference and set to NULL after being destroyed.
/// @param freeFunc Function called on each value, or NULL.
void
MTY_HashDestroy(MTY_Hash **hash, MTY_FreeFunc freeFunc);

/// @brief Get a value from an MTY_Hash (NULL if `key` is not there).
void *
MTY_HashGet(MTY_Hash *ctx, const char *key);

/// @brief Get a value from an MTY_Hash via an integer key.
void *
MTY_HashGetInt(MTY_Hash *ctx, int64_t key);

/// @brief Set a value in an MTY_Hash: returns the value it replaced, or NULL.
void *
MTY_HashSet(MTY_Hash *ctx, const char *key, void *value);

/// @brief Set a value in an MTY_Hash via an integer key: returns the value it replaced, or NULL.
void *
MTY_HashSetInt(MTY_Hash *ctx, int64_t key, void *value);

/// @brief Remove a value from an MTY_Hash and return it (NULL if `key` is not there).
void *
MTY_HashPop(MTY_Hash *ctx, const char *key);

/// @brief Remove a value from an MTY_Hash via an integer key and return it.
void *
MTY_HashPopInt(MTY_Hash *ctx, int64_t key);

/// @brief Iterate through the keys of an MTY_Hash (`iter` set to 0 before the first call).
bool
MTY_HashGetNextKey(MTY_Hash *ctx, uint64_t *iter, const char **key);

/// @brief Iterate through the integer keys of an MTY_Hash (`iter` set to 0 before the first call).
bool
MTY_HashGetNextKeyInt(MTY_Hash *ctx, uint64_t *iter, int64_t *key);


// The libmatoya helpers the two files call (mty.c).

void *MTY_Alloc(size_t len, size_t size);
void *MTY_Realloc(void *mem, size_t len, size_t size);
void MTY_Free(void *mem);
char *MTY_Strdup(const char *str);
void MTY_Log(const char *fmt, ...);
uint32_t MTY_DJB2(const char *str);

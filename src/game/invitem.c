#include "invitem.h"

#include <stddef.h>

#include "../base/byteorder.h"
#include "../base/cstr.h"
#include "../data/docfile.h"
#include "invlist.h"
#include "../base/memory.h"
#include "../data/strrsrc.h"

const struct inv_item_ops INV_ITEM_OPS_TABLE = {
	{inv_item_destroy},
	inv_item_total_weight,
	inv_item_get_unit_weight_x16,
};

static const struct item_record *item_record(struct inv_item *self)
{
	const struct item_table *table = GetItemTable();

	if (!table || self->table_index < 0 || self->table_index >= table->count)
		return NULL;

	return &table->records[self->table_index];
}

void InvItemInit(struct inv_item *self, const struct cstr *nm, int32_t qty, double price)
{
	ObjectInit(&self->base);
	self->base.ops = &INV_ITEM_OPS_TABLE.base;
	self->node.prev = NULL;
	self->node.next = NULL;
	self->name = CStrCreateCopy(nm);
	self->quantity = qty;
	self->partial_portions = 0;

	struct strrsrc *keys = GetItemKeyNames();

	self->table_index = keys ? StrRsrcIndexOf(keys, CStrGet(nm)) : -1;

	if (self->table_index == -1) {
		self->days_to_spoil = 0;

	} else {
		const struct item_record *rec = item_record(self);
		self->days_to_spoil = rec ? rec->shelf_life_days : 0;
	}

	self->selected_qty = 0;
	self->price_factor = price;
}

void InvItemInitEmpty(struct inv_item *self)
{
	ObjectInit(&self->base);
	self->base.ops = &INV_ITEM_OPS_TABLE.base;
	self->node.prev = NULL;
	self->node.next = NULL;
	self->name = CStrCreate();
}

struct inv_item *InvItemCreate(const struct cstr *nm, int32_t qty, double price)
{
	struct inv_item *self = MemAlloc(1, sizeof(struct inv_item));
	InvItemInit(self, nm, qty, price);

	return self;
}

struct inv_item *InvItemCreateEmpty(void)
{
	struct inv_item *self = MemAlloc(1, sizeof(struct inv_item));
	InvItemInitEmpty(self);

	return self;
}

void InvItemDestroy(struct inv_item **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void inv_item_destroy(struct object *self)
{
	struct inv_item *item = (struct inv_item *) self;

	CStrDestroy(&item->name);

	object_destroy(self);
}

struct inv_item *InvItemFromNode(struct list_node *node)
{
	if (!node)
		return NULL;

	return (struct inv_item *) ((uint8_t *) node - offsetof(struct inv_item, node));
}

struct cstr *InvItemGetNameCStr(struct inv_item *self)
{
	return self->name;
}

struct cstr *InvItemGetName(struct inv_item *self)
{
	if (self->table_index == -1)
		return CStrCreateCopy(self->name);

	return self->quantity == 1 ? StrRsrcGetCStr(GetItemSingularNames(), self->table_index)
		: StrRsrcGetCStr(GetItemPluralNames(), self->table_index);
}

struct cstr *InvItemGetNamePlural(struct inv_item *self)
{
	if (self->table_index == -1)
		return CStrCreateCopy(self->name);

	return StrRsrcGetCStr(GetItemPluralNames(), self->table_index);
}

struct cstr *InvItemGetNameSingular(struct inv_item *self)
{
	if (self->table_index == -1)
		return CStrCreateCopy(self->name);

	return StrRsrcGetCStr(GetItemSingularNames(), self->table_index);
}

bool InvItemInGroup(struct inv_item *self, int32_t group)
{
	if (self->table_index == -1)
		return false;

	const struct item_record *rec = item_record(self);

	if (!rec)
		return false;

	bool found = false;

	for (int32_t i = 0; i < 8 && rec->groups[i]; ++i) {
		if ((signed char) rec->groups[i] == group) {
			found = true;
			break;
		}
	}

	return found;
}

bool InvItemInCategory(struct inv_item *self, int32_t subtype)
{
	if (self->table_index == -1)
		return false;

	const struct item_record *rec = item_record(self);
	bool match = false;

	if (rec && rec->category == subtype)
		match = true;

	return match;
}

int32_t InvItemConsumePortions(struct inv_item *self, int32_t weight)
{
	int32_t remaining = weight;
	int32_t divisor = InvItemGetPortionsPerUnit(self);

	if (divisor == 0)
		return weight;

	if (self->partial_portions == 0 && self->quantity != 0) {
		self->partial_portions = divisor;
		self->quantity--;
	}

	while (self->partial_portions < remaining && self->quantity != 0) {
		remaining -= self->partial_portions;
		self->partial_portions = divisor;
		self->quantity--;
	}

	if (self->partial_portions < remaining) {
		remaining -= self->partial_portions;
		self->partial_portions = 0;

	} else if (self->partial_portions == remaining) {
		self->partial_portions = 0;
		remaining = self->partial_portions;

	} else {
		self->partial_portions -= remaining;
		remaining = 0;
	}

	return weight - remaining;
}

int32_t InvItemTotalValue(struct inv_item *self)
{
	double unit_price = (double) InvItemGetUnitPrice(self);
	double subtotal = (double) self->quantity * unit_price;

	if (self->partial_portions) {
		double divisor = (double) InvItemGetPortionsPerUnit(self);

		if (divisor == 0.0)
			divisor = 1.0;

		double partial_fraction = (double) self->partial_portions / divisor;
		subtotal = partial_fraction * unit_price + subtotal;
	}

	return (int32_t) subtotal;
}

int32_t InvItemGetUnitPrice(struct inv_item *self)
{
	if (self->table_index == -1)
		return 0;

	double price_scale;

	if (self->price_factor != 0.0) {
		price_scale = self->price_factor;

	} else {
		price_scale = 1.0;
	}

	const struct item_record *rec = item_record(self);
	double price = rec ? (double) rec->price_cents : 0.0;

	return (int32_t) (price * price_scale);
}

int32_t inv_item_total_weight(struct inv_item *self)
{
	if (self->table_index == -1)
		return 0;

	double per_unit = (double) INV_ITEM_OPS(self)->GetUnitWeightX16(self);
	double total = (double) self->quantity * per_unit;

	if (self->partial_portions) {
		double divisor = (double) InvItemGetPortionsPerUnit(self);

		if (divisor == 0.0)
			divisor = 1.0;

		double frac = (double) self->partial_portions / divisor;
		total = frac * per_unit + total;
	}

	return (int32_t) total;
}

int32_t inv_item_get_unit_weight_x16(struct inv_item *self)
{
	if (self->table_index == -1)
		return 0;

	const struct item_record *rec = item_record(self);

	return rec ? rec->weight_x16 : 0;
}

void InvItemAddUnits(struct inv_item *self, int32_t n)
{
	self->quantity += n;
}

bool InvItemRemoveUnits(struct inv_item *self, int32_t n)
{
	if (self->quantity <= n) {
		self->quantity = 0;
		self->partial_portions = 0;
		return false;
	}

	self->quantity -= n;

	return true;
}

bool InvItemIsPerishable(struct inv_item *self)
{
	return self->days_to_spoil >= 0;
}

bool InvItemTickSpoilage(struct inv_item *self)
{
	if (InvItemIsPerishable(self)) {
		self->days_to_spoil--;

		if (self->days_to_spoil == 0)
			return true;
	}

	return false;
}

int32_t InvItemGetCount(struct inv_item *self, bool raw)
{
	if (raw)
		return self->quantity;

	if (InvItemGetPartialPortions(self))
		return self->quantity + 1;

	return self->quantity;
}

int32_t InvItemGetPartialPortions(struct inv_item *self)
{
	return self->partial_portions;
}

int32_t InvItemGetPortionsPerUnit(struct inv_item *self)
{
	if (self->table_index == -1)
		return 0;

	const struct item_record *rec = item_record(self);

	return rec ? rec->portions : 0;
}

int32_t InvItemTotalPortions(struct inv_item *self)
{
	if (self->table_index == -1)
		return 0;

	return InvItemGetPortionsPerUnit(self) * self->quantity + self->partial_portions;
}

static int32_t block_word(const uint8_t *blk, int32_t size, int32_t i)
{
	if (!blk || 2 * i + 2 > size)
		return 0;

	return ReadLe16(blk + 2 * i);
}

void InvItemLoadState(struct inv_item *self, struct doc_file *file)
{
	void *mem = NULL;
	int32_t size = 0;

	DocFileReadObject2(file, &mem, &size);
	const uint8_t *blk = mem;

	self->table_index = block_word(blk, size, 0);
	self->quantity = block_word(blk, size, 1);
	self->partial_portions = block_word(blk, size, 2);
	self->days_to_spoil = block_word(blk, size, 3);
	self->selected_qty = block_word(blk, size, 4);
	self->price_factor = (double) block_word(blk, size, 5) / 100.0;

	struct strrsrc *keys = GetItemKeyNames();
	struct cstr *key_name = keys ? StrRsrcGetCStr(keys, self->table_index) : NULL;

	if (key_name)
		CStrAssign(self->name, key_name);

	CStrDestroy(&key_name);

	MemFree(mem);
}

void InvItemSaveState(struct inv_item *self, struct doc_file *file)
{
	uint8_t blk[14] = {0};

	WriteLe16(blk + 0, self->table_index);
	WriteLe16(blk + 2, self->quantity);
	WriteLe16(blk + 4, self->partial_portions);
	WriteLe16(blk + 6, self->days_to_spoil);
	WriteLe16(blk + 8, self->selected_qty);

	double cents = self->price_factor * 100.0 + (self->price_factor < 0 ? -0.5 : 0.5);
	WriteLe16(blk + 10, cents > 32767.0 ? 32767 : cents < -32768.0 ? -32768 : (int32_t) cents);

	DocFileWriteObject(file, blk, 14);
}

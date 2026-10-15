#include "pcmdecode.h"

void DeltaDecode(uint8_t state[4], int32_t index, int32_t count, uint8_t *buf)
{
	uint8_t *deltas = state;

	switch (index) {
		case 0:
			break;

		case 1:
			while (--count >= 0) {
				deltas[0] = (uint8_t) (deltas[0] + buf[0] + 0x80);
				buf[0] = deltas[0];
				buf++;
			}
			break;

		case 2:
			while (--count >= 0) {
				deltas[1] = (uint8_t) (deltas[1] + buf[0] + 0x80);
				deltas[0] = (uint8_t) (deltas[0] + deltas[1] + 0x80);
				buf[0] = deltas[0];
				buf++;
			}
			break;

		case 3:
			while (--count >= 0) {
				deltas[2] = (uint8_t) (deltas[2] + buf[0] + 0x80);
				deltas[1] = (uint8_t) (deltas[2] + deltas[1] + 0x80);
				deltas[0] = (uint8_t) (deltas[0] + deltas[1] + 0x80);
				buf[0] = deltas[0];
				buf++;
			}
			break;

		case 4:
			while (--count >= 0) {
				deltas[3] = (uint8_t) (deltas[3] + buf[0] + 0x80);
				deltas[2] = (uint8_t) (deltas[2] + deltas[3] + 0x80);
				deltas[1] = (uint8_t) (deltas[2] + deltas[1] + 0x80);
				deltas[0] = (uint8_t) (deltas[0] + deltas[1] + 0x80);
				buf[0] = deltas[0];
				buf++;
			}
			break;
	}
}

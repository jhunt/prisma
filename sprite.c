#include "prisma.h"

int sprite_moving(struct sprite *sprite)
{
	return sprite->delta.x || sprite->delta.y;
}

int sprite_tile(struct sprite *sprite)
{
	int t;

	if (!sprite_moving(sprite)) {
		sprite->frame = 0;
		return 0;
	}

	t = sprite->frame + 1;
	t += sprite->delta.x > 0 ? 3  /* RIGHT */
	   : sprite->delta.x < 0 ? 6  /* LEFT */
	   : sprite->delta.y < 0 ? 9  /* UP */
	   :                       0; /* DOWN */

	return t;
}

#define MOVE_DELTA 8
void sprite_move_x(struct sprite *sprite, int x)
{
	sprite->delta.x = x * MOVE_DELTA;
}

void sprite_move_y(struct sprite *sprite, int y)
{
	sprite->delta.y = y * MOVE_DELTA;
}

void sprite_move_all(struct sprite *sprite, int left, int right, int up, int down)
{
	sprite->delta.x = 0;
	sprite->delta.y = 0;
	if (left)  sprite->delta.x = -1 * MOVE_DELTA;
	if (right) sprite->delta.x =  1 * MOVE_DELTA;
	if (up)    sprite->delta.y = -1 * MOVE_DELTA;
	if (down)  sprite->delta.y =  1 * MOVE_DELTA;
}

#include <SDL.h>

int main(int argc, char **argv)
{
	int n, i;
	SDL_Joystick *joy;

	SDL_Init(SDL_INIT_JOYSTICK);

	n = SDL_NumJoysticks();
	printf("%d joysticks found\n", n);
	for (i = 0; i < n; i++) {
		joy = SDL_JoystickOpen(i);
		printf("[%d] %s\n", i, SDL_JoystickName(joy));
		if (SDL_JoystickGetAttached(joy)) {
			SDL_JoystickClose(joy);
		}
	}
	return 0;
}

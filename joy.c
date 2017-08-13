#include <SDL.h>
#include <signal.h>

#define MAX_JOYSTICKS 8

int done = 0;

static void sigterm(int sig) {
	done = 1;
}

int main(int argc, char **argv)
{
	int n, i;
	SDL_Joystick *joy;
	SDL_Joystick *all[MAX_JOYSTICKS];
	SDL_Event e;

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

	signal(SIGTERM, sigterm);
	signal(SIGINT,  sigterm);

	if (n > MAX_JOYSTICKS) {
		fprintf(stderr, "WARNING: found %d joysticks;\n"
		                "only considering the first %d...\n",
		                n, MAX_JOYSTICKS);
		n = MAX_JOYSTICKS;
	}
	while (n--) {
		all[n] = SDL_JoystickOpen(n);
	}
	while (!done) {
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
			case SDL_JOYDEVICEADDED:
				fprintf(stderr, "NEW CONTROLLER FOUND!\n");
				if (e.jdevice.which > MAX_JOYSTICKS) {
					fprintf(stderr, "index %d out of range (> %d); IGNORING\n",
					                e.jdevice.which, MAX_JOYSTICKS);
				} else {
					joy = SDL_JoystickOpen(e.jdevice.which);
					fprintf(stderr, "[%d] %s\n", e.jdevice.which, SDL_JoystickName(joy));
					all[e.jdevice.which] = joy;
				}
				break;
			case SDL_JOYDEVICEREMOVED:
				fprintf(stderr, "controller[%d] disconnected\n", e.jdevice.which);
				SDL_JoystickClose(all[e.jdevice.which]);
			case SDL_JOYBUTTONDOWN:
				fprintf(stderr, "button[%d] pressed %d\n", e.jbutton.button, e.jbutton.state);
				break;
			case SDL_JOYBUTTONUP:
				fprintf(stderr, "button[%d] released %d\n", e.jbutton.button, e.jbutton.state);
				break;
			case SDL_JOYHATMOTION:
				fprintf(stderr, "hat[%d] %d\n", e.jhat.hat, e.jhat.value);
				break;
			case SDL_JOYAXISMOTION:
				fprintf(stderr, "axis[%d] %d\n", e.jaxis.axis, e.jaxis.value);
				break;
			}
		}
	}
	fprintf(stderr, "\rterminating...\n");
	return 0;
}

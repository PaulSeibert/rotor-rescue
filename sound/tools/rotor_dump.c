// Host tool for make_rotor.py --check: writes logicRotorSynth's loop to stdout (ROTOR_SAMPLES bytes).
//   gcc -I src -o /tmp/rotor_dump sound/tools/rotor_dump.c src/logic.c src/hiscore.c
#include <stdio.h>
#include "logic.h"

int main(void) {
	static int8_t s_pLoop[ROTOR_SAMPLES];
	logicRotorSynth(s_pLoop);
	fwrite(s_pLoop, 1, ROTOR_SAMPLES, stdout);
	return 0;
}

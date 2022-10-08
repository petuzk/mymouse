#include <avr/io.h>
#include <avr/sleep.h>

// -U lfuse:w:0xE2:m -U hfuse:w:0xDF:m -U efuse:w:0xFE:m

int main() {
    // disable internal 3v3 regulator as per note in figure 20-6
    REGCR = (1 << REGDIS);
    // disable analog comparator
    ACSR = (1 << ACD);
    // disable timers and USART
    PRR0 = (1 << PRTIM0) | (1 << PRTIM1);
    PRR1 = (1 << PRUSART1);

    // enter power down
    SMCR = SLEEP_MODE_PWR_DOWN | (1 << SE);
    sleep_cpu(); // asm "sleep" instruction

    while(1);
    return 0;
}

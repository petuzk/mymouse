#include <stdbool.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <util/crc16.h>

#define PIN_IRQ PB4
#define DDR_IRQ DDRB

// number of remaining bytes to send/receive
static uint8_t num_tx = 0;
static uint8_t num_rx = 0;
// data buffers
static uint8_t tx_data[16];  // first byte not included as it's sent right away
static uint8_t rx_data[16];
// index to the buffer
static uint8_t tx_idx = 0;
static uint8_t rx_idx = 0;

static uint8_t tx_crc = 0;

void init_spi() {
    // set C6 low and make it an output -- this enables MISO voltage divider
    PORTC &= ~(1 << PC6);
    DDRC |= 1 << PC6;

    // set CS, IRQ as inputs
    DDRB = 0;
}

static void enable_spi() {
    DDRB |= 1 << PB3;  // set B3 (MISO) as output
    SPCR = 0
        | (1 << SPE)   // enable SPI
        | (0 << MSTR)  // slave mode
        | (0 << DORD)  // MSB is transmitted first
        | (1 << CPOL)  // SCK is high when inactive
        | (1 << CPHA)  // sample on trailing edge
    ;

    // set IRQ as output low
    DDR_IRQ |= (1 << PIN_IRQ);
}

static inline uint8_t is_cs_high() {
    return PINB & (1 << PB0);
}

static uint8_t prepare_response(const uint8_t command_id) {
    uint8_t first_tx_byte = 0;
    switch (command_id) {
        case 0x06:
            first_tx_byte = SPDR = 0x1E;
            tx_data[0] = 0x93;
            tx_data[1] = 0x89;
            num_tx = 2;
            break;
    }
    return first_tx_byte;
}

static inline uint8_t get_next_tx_byte() {
    if (tx_idx < num_tx) {
        uint8_t next_byte = tx_data[tx_idx++];
        tx_crc = _crc_ibutton_update(tx_crc, next_byte);
        return next_byte;
    }
    return tx_crc;
}

static inline void store_rx_byte(const uint8_t value) {
    if (rx_idx < num_rx) {
        rx_data[rx_idx++] = value;
    }
}

static void process_transaction(const uint8_t command_id) {
    // TBD
}

static inline uint8_t wait_for_command_id() {
    // wait for up to 1 ms to receive command id
    uint16_t wait_counter = 1000;
    while (!(SPSR & (1 << SPIF)) && wait_counter--) {
        _delay_us(1);
    }
    return (SPSR & (1 << SPIF)) ? SPDR : 0;
}

static inline void transceive_byte(const register uint8_t tx_byte) {
    // wait for the current byte to be fully clocked out, set TX and store RX
    // the SPDR has a very narrow time window to be set, because it must happen during one SPI CLK period
    // (after previous byte is clocked out, but before the new one is)
    // to keep up with this hard timing requirement, it is essentially "spammed" in the loop below
    cli();
    while (true) {
        SPDR = tx_byte;
        if (SPSR & (1 << SPIF)) {
            // finished clocking out one byte, set TX and store RX
            SPDR = tx_byte;
            store_rx_byte(SPDR);
            break;
        }
        SPDR = tx_byte;
        if (is_cs_high()) {
            // CS went high, stop data transfer and don't save RX
            break;
        }
        SPDR = tx_byte;
    }
    sei();
}

void spi_task() {
    // SPI should only be enabled once nRF sets CS high
    if (!(SPCR & (1 << SPE))) {
        // SPI is not enabled yet, check if CS is high
        if (is_cs_high()) {
            // CS is high, enable SPI
            enable_spi();
        }
        return;
    }
    if (!is_cs_high()) {
        const uint8_t command_id = wait_for_command_id();
        if (!command_id) {
            return;
        }
        // prepare response (tx and rx buffers)
        const uint8_t first_tx_byte = prepare_response(command_id);

        tx_crc = _crc_ibutton_update(0, first_tx_byte);

        // wait for transfer to begin by detecting WCOL (write collision) bit
        do {
            SPDR = first_tx_byte;
        } while (!(SPSR & (1 << WCOL)) && !is_cs_high());

        // main data transfer loop
        while (!is_cs_high()) {
            // the loop is always entered shortly after a new 8-byte clock cycle started -- clear previous flags
            SPSR |= (1 << WCOL) | (1 << SPIF);
            transceive_byte(get_next_tx_byte());
            wdt_reset();
        }
        // if there was data received, and the byte amount is correct, process command result
        if (num_rx && rx_idx == num_rx && tx_idx == num_tx) {
            process_transaction(command_id);
        }

        // cleanup
        num_tx = num_rx = 0;
        tx_idx = rx_idx = 0;
        SPSR |= (1 << WCOL) | (1 << SPIF);
    }
}

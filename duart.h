#ifndef DUART_H
#define DUART_H

#include <stdint.h>

// MC68681 config
#define DUART_BASE 0x800181                 /* Base of I/O port addresses */

/* -----------------------------------------------------------------------
 * Register definitions
 * Same physical address, read vs write selects the register (per Table 4-1)
 * ----------------------------------------------------------------------- */

#define MR1A  (* (volatile char *) (DUART_BASE+0) )    /* Mode Register 1A          (R/W) */
#define MR2A  (* (volatile char *) (DUART_BASE+0) )    /* Mode Register 2A          (R/W) */

#define SRA   (* (volatile char *) (DUART_BASE+2) )    /* Status Register A         (R)   */
#define CSRA  (* (volatile char *) (DUART_BASE+2) )    /* Clock Select Register A   (W)   */

                                                        /* +4: Do Not Access on read       */
#define CRA   (* (volatile char *) (DUART_BASE+4) )    /* Command Register A        (W)   */

#define RBRA  (* (volatile char *) (DUART_BASE+6) )    /* Receiver Buffer A         (R)   */
#define TBRA  (* (volatile char *) (DUART_BASE+6) )    /* Transmitter Buffer A      (W)   */

#define IPCR  (* (volatile char *) (DUART_BASE+8) )    /* Input Port Change Reg     (R)   */
#define ACR   (* (volatile char *) (DUART_BASE+8) )    /* Auxiliary Control Reg     (W)   */

#define ISR   (* (volatile char *) (DUART_BASE+10) )   /* Interrupt Status Reg      (R)   */
#define IMR   (* (volatile char *) (DUART_BASE+10) )   /* Interrupt Mask Reg        (W)   */

#define CUR   (* (volatile char *) (DUART_BASE+12) )   /* Counter MSB (read)        (R)   */
#define CTUR  (* (volatile char *) (DUART_BASE+12) )   /* Counter/Timer Upper Reg   (W)   */

#define CLR   (* (volatile char *) (DUART_BASE+14) )   /* Counter LSB (read)        (R)   */
#define CTLR  (* (volatile char *) (DUART_BASE+14) )   /* Counter/Timer Lower Reg   (W)   */

#define MR1B  (* (volatile char *) (DUART_BASE+16) )   /* Mode Register 1B          (R/W) */
#define MR2B  (* (volatile char *) (DUART_BASE+16) )   /* Mode Register 2B          (R/W) */

#define SRB   (* (volatile char *) (DUART_BASE+18) )   /* Status Register B         (R)   */
#define CSRB  (* (volatile char *) (DUART_BASE+18) )   /* Clock Select Register B   (W)   */

                                                        /* +20: Do Not Access on read      */
#define CRB   (* (volatile char *) (DUART_BASE+20) )   /* Command Register B        (W)   */

#define RBRB  (* (volatile char *) (DUART_BASE+22) )   /* Receiver Buffer B         (R)   */
#define TBRB  (* (volatile char *) (DUART_BASE+22) )   /* Transmitter Buffer B      (W)   */

#define IVR   (* (volatile char *) (DUART_BASE+24) )   /* Interrupt Vector Reg      (R/W) */

#define IPUL  (* (volatile char *) (DUART_BASE+26) )   /* Unlatched Input Port      (R)   */
#define OPCR  (* (volatile char *) (DUART_BASE+26) )   /* Output Port Config Reg    (W)   */

#define STRT_CNTR (* (volatile char *) (DUART_BASE+28) ) /* Start Counter (R) / OPR Set (W) */
#define OPR_SET   (* (volatile char *) (DUART_BASE+28) ) /* Output Port Bit Set Cmd  (W)   */

#define STOP_CNTR (* (volatile char *) (DUART_BASE+30) ) /* Stop Counter / clr CT irq (R)  */
#define OPR_CLR   (* (volatile char *) (DUART_BASE+30) ) /* Output Port Bit Clear Cmd (W)  */

/* -----------------------------------------------------------------------
 * SRA / SRB status bits
 * ----------------------------------------------------------------------- */
#define RxRDY   0x01    /* Receiver ready                   SRA/SRB bit 0 */
#define FFULL   0x02    /* FIFO full                        SRA/SRB bit 1 */
#define TxRDY   0x04    /* Transmitter ready (THR empty)    SRA/SRB bit 2 */
#define TxEMT   0x08    /* Transmitter empty (shift reg)    SRA/SRB bit 3 */
#define OVR_ERR 0x10    /* Overrun error                    SRA/SRB bit 4 */
#define PAR_ERR 0x20    /* Parity error                     SRA/SRB bit 5 */
#define FRM_ERR 0x40    /* Framing error                    SRA/SRB bit 6 */
#define RCV_BRK 0x80    /* Received break                   SRA/SRB bit 7 */

/* -----------------------------------------------------------------------
 * ISR / IMR bits  (Interrupt Status / Mask Register)
 * Bit positions are identical in both registers (Table 4-12)
 * ----------------------------------------------------------------------- */
#define ISR_TxRDYA  0x01    /* Channel A transmitter ready      ISR bit 0 */
#define ISR_RxRDYA  0x02    /* Channel A receiver ready/FFULL   ISR bit 1 */
#define ISR_BRKA    0x04    /* Channel A delta break            ISR bit 2 */
#define ISR_CTRDY   0x08    /* Counter/timer ready              ISR bit 3 */
#define ISR_TxRDYB  0x10    /* Channel B transmitter ready      ISR bit 4 */
#define ISR_RxRDYB  0x20    /* Channel B receiver ready/FFULL   ISR bit 5 */
#define ISR_BRKB    0x40    /* Channel B delta break            ISR bit 6 */
#define ISR_IPCNG   0x80    /* Input port change                ISR bit 7 */

/* Legacy aliases used in original ISR code */
#define iRxRDY  ISR_RxRDYA
#define iCTRDY  ISR_CTRDY

/* -----------------------------------------------------------------------
 * CRA / CRB command register helpers
 *
 * Write a single byte built from the three fields:
 *   bits[6:4]  miscellaneous command
 *   bits[3:2]  transmitter command
 *   bits[1:0]  receiver command
 *
 * Use 0 for any field you want to leave as "no action".
 * ----------------------------------------------------------------------- */

/* Miscellaneous commands (bits[6:4]) */
#define CR_MISC_NOP         0x00    /* No command                          */
#define CR_RESET_MRP        0x10    /* Reset mode-register pointer to MR1  */
#define CR_RESET_RX         0x20    /* Reset receiver                      */
#define CR_RESET_TX         0x30    /* Reset transmitter                   */
#define CR_RESET_ERR        0x40    /* Reset error status                  */
#define CR_RESET_BRK        0x50    /* Reset break-change interrupt        */
#define CR_START_BRK        0x60    /* Start break                         */
#define CR_STOP_BRK         0x70    /* Stop break                          */

/* Transmitter commands (bits[3:2]) */
#define CR_TX_NOP           0x00    /* No action                           */
#define CR_TX_ENABLE        0x04    /* Enable transmitter                  */
#define CR_TX_DISABLE       0x08    /* Disable transmitter                 */

/* Receiver commands (bits[1:0]) */
#define CR_RX_NOP           0x00    /* No action                           */
#define CR_RX_ENABLE        0x01    /* Enable receiver                     */
#define CR_RX_DISABLE       0x02    /* Disable receiver                    */

/* -----------------------------------------------------------------------
 * ACR – Auxiliary Control Register
 *
 * Bit 7:    BRG set select (0 = set 1, 1 = set 2)
 * Bits[6:4]: Counter/timer mode and clock source (Table 4-4)
 * Bits[3:0]: IP change-of-state interrupt enable (IP3..IP0)
 * ----------------------------------------------------------------------- */

/* BRG set */
#define ACR_BRG_SET1    0x00    /* Baud-rate generator set 1 (ACR[7]=0) */
#define ACR_BRG_SET2    0x80    /* Baud-rate generator set 2 (ACR[7]=1) */

/*
 * Counter/Timer mode + clock source field (ACR[6:4])
 * Refer to Table 4-4 in the datasheet.
 *
 * Counter mode (ACR[6]=0):
 *   000 – external clock on IP2
 *   001 – TxCA (channel A transmitter 1X clock)
 *   010 – TxCB (channel B transmitter 1X clock)
 *   011 – crystal/external clock (X1) ÷ 16
 *
 * Timer mode (ACR[6]=1):
 *   100 – external clock on IP2
 *   101 – external clock on IP2 ÷ 16
 *   110 – crystal/external clock (X1)
 *   111 – crystal/external clock (X1) ÷ 16
 */
#define ACR_CT_COUNTER_IP2      0x00
#define ACR_CT_COUNTER_TXCA     0x10
#define ACR_CT_COUNTER_TXCB     0x20
#define ACR_CT_COUNTER_X1D16    0x30

#define ACR_CT_TIMER_IP2        0x40
#define ACR_CT_TIMER_IP2D16     0x50
#define ACR_CT_TIMER_X1         0x60
#define ACR_CT_TIMER_X1D16      0x70    /* Most common: X1/16 timer  */

/* IP change-of-state enable bits (ACR[3:0]) */
#define ACR_IP0_CHG     0x01
#define ACR_IP1_CHG     0x02
#define ACR_IP2_CHG     0x04
#define ACR_IP3_CHG     0x08

/* -----------------------------------------------------------------------
 * CSRA / CSRB – Clock Select Register
 *
 * Bits[7:4] = receiver clock, bits[3:0] = transmitter clock.
 * Both nibbles use the same encoding (Table in Section 4.2).
 * With BRG set 1 and 3.6864 MHz crystal:
 *
 *   0x00 = 50 baud       0x44 = 300 baud    0x88 = 2400 baud
 *   0x11 = 110 baud      0x55 = 600 baud    0x99 = 4800 baud
 *   0x33 = 200 baud      0x66 = 1200 baud   0xBB = 9600 baud
 *   0xCC = 38400 baud    0xDD = timer       0xEE = IPn-16X
 *   0xFF = IPn-1X
 * ----------------------------------------------------------------------- */
#define CSR_50          0x00
#define CSR_110         0x11
#define CSR_134_5       0x22
#define CSR_200         0x33
#define CSR_300         0x44
#define CSR_600         0x55
#define CSR_1200        0x66
#define CSR_2400        0x88
#define CSR_4800        0x99
#define CSR_9600        0xBB
#define CSR_38400       0xCC
#define CSR_TIMER       0xDD    /* Use counter/timer output as baud clock  */

/* -----------------------------------------------------------------------
 * Counter/Timer helpers
 *
 * With timer mode and X1/16 clock source (ACR_CT_TIMER_X1D16):
 *   clock_in = 3.6864 MHz / 16 = 230400 Hz
 *   period   = 2 * preload / clock_in
 *   preload  = period * clock_in / 2
 *
 * Example: 1 ms tick
 *   preload = 0.001 * 230400 / 2 = 115 = 0x0073
 *
 * Example: 10 ms tick
 *   preload = 0.010 * 230400 / 2 = 1152 = 0x0480
 *
 * Macro: CT_PRELOAD_MS(ms) – compute preload for a given millisecond period.
 * Assumes X1 = 3.6864 MHz and ACR_CT_TIMER_X1D16 clock source.
 * ----------------------------------------------------------------------- */
#define CT_CLK_HZ           230400UL    /* 3686400 / 16 */
#define CT_PRELOAD_MS(ms)   (((ms) * CT_CLK_HZ) / 2000UL)
#define CT_PRELOAD_US(us)   (((us) * CT_CLK_HZ) / 2000000UL)

/* Convenience: split a 16-bit preload into upper / lower bytes */
#define CT_UPPER(val)   (((val) >> 8) & 0xFF)
#define CT_LOWER(val)   ((val) & 0xFF)

#endif /* DUART_H */

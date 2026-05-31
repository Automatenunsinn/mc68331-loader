#include <stdint.h>
#include <stdbool.h>
#include "duart.h"
#include "mc68331.h"

// Version selection
//
// KSA mixing differs per variant. Disassembly comparison:
//   variant   USP scramble   per-i mix
//   5.0a      none           (none)
//   5.0b 1MB  0xeada1274     ^ ~i ^ (i<<2)
//   5.0b 2MB  0x2378bf41     ^  i ^ (i<<2)
//   30ALT     0xcac0facc     ^ ~i ^ (i<<1)
//   UHG       (uses ENC_TABLE only — no NVRAM, no USP)
//   ROTE      none           (none)
#if defined(VERSION_50B_1MB)
#include "table_rd.h"
#define HEADER_MAGIC 0x61640300 //ad
#define HAS_WARM_BOOT   1
#define HAS_RC4_NMIX    1
#define KSA_USP_SCRAMBLE 0xeada1274u
#define KSA_MIX_NOT_I    1
#define KSA_MIX_I_SHIFT  2
#elif defined(VERSION_50B_2MB)
#include "table_rd.h"
#define HEADER_MAGIC 0x61640400 //ad
#define HAS_WARM_BOOT   1
#define HAS_RC4_NMIX    1
#define KSA_USP_SCRAMBLE 0x2378bf41u
#define KSA_MIX_I        1
#define KSA_MIX_I_SHIFT  2
#elif defined(VERSION_50A)
#include "table_rd.h"
#define HEADER_MAGIC 0x31415900 //1AY
#define HAS_RC4_NMIX    1
#elif defined(VERSION_30ALT)
#include "table_rd.h"
#define HEADER_MAGIC 0x61647000 //adp
#define HAS_RC4_NMIX    1
#define HAS_EXTRA_SIG_SCAN 1  // scans for 0x4afa4afa marker at NVRAM[0x58]
#define KSA_USP_SCRAMBLE 0xcac0faccu
#define KSA_MIX_NOT_I    1
#define KSA_MIX_I_SHIFT  1
#define STACK_DETECT_100C 1   // size stack from magic long at NVRAM 0x100c
#define CS7_UNCONDITIONAL 1   // CSBAR7/CSOR7 always written (no 0x200000 gate)
#define EXTRA_FFF9_REGS   1   // also clear fff90c/920/924/926
#elif defined(VERSION_UHG)
#include "table_old.h"
#define HEADER_MAGIC 0x53746c00 //Stl
#define OLD_FAMILY      1  // no RC4+NMIX crypt, no process_header
#define UHG_KSA         1  // classic RC4 KSA keyed by static ENC_TABLE (mod 64)
#define CS7_UNCONDITIONAL 1
#define EXTRA_FFF9_REGS   1
#elif defined(VERSION_ROTE)
#include "table_old.h"
#define HEADER_MAGIC 0x31415900 //1AY
#define OLD_FAMILY      1  // no RC4+NMIX crypt, no process_header
#define CS7_UNCONDITIONAL 1
#define EXTRA_FFF9_REGS   1
#else
#include "table_zero.h" // Default if none specified
#define HEADER_MAGIC 0x4e4f4e45 //NONE
#endif

const uint8_t MAGIC1[] = {'R', 'X', 'M', 'B', 'R', 'X', 'M', 'B', 'V', 0x40, 'H', 'S', 'F', 'N', 0x09};
const uint8_t MAGIC2[] = {'j', 'h', 'k', 'k', 0xfc, 0xc3, 0x54, 0x1a, 'R', 'X', 'M', 'B', 'R', 'X', 'M', 'B', 'V', 0x40, 'H', 'S', 'F', 'N', 0x09};

// Prototypes
void putchar_(char c);
void control_led_and_send(uint8_t value);
void init_duart(uint32_t slow_mode);
void main_task(void);
void process_communication(void);
void update_state_bit(uint8_t value, uint8_t bit_count);
void process_header(uint8_t *header);
bool check_magic(void);
uint8_t verify_checksum(void);
void reset_and_wait(uint32_t led_pattern);

// RC4 functions
void rc4_ksa(uint8_t *state, uint8_t *key_table, uint8_t *i_ptr, uint8_t *j_ptr);
uint8_t rc4_crypt_byte(uint32_t data_byte, uint8_t *state, uint8_t *i_ptr, uint8_t *j_ptr);

#define STATE_REG (* (volatile uint8_t *) (0xfff907) )

void update_state_bit(uint8_t value, uint8_t bit_count)
{
    uint8_t mask = 0x01;

    while (bit_count--)
    {
        STATE_REG &= ~0x10; // Clear bit 4
        if (value & mask)
        {
            STATE_REG |= 0x80; // Set bit 7
        }
        else
        {
            STATE_REG &= ~0x80; // Clear bit 7
        }
        STATE_REG |= 0x10; // Set bit 4
        mask <<= 1;
    }
    STATE_REG &= ~0x10;
}

#ifndef OLD_FAMILY
void process_header(uint8_t *header)
{
    const uint8_t *p = ENC_TABLE;

    // Original hardware init at start of process_header
    (*(volatile uint16_t *)(0xFFF906)) = 0xf000;
    STATE_REG = 0x00;
    STATE_REG |= 0x20;
    STATE_REG |= 0x40;

    while (*p != 0xFF)
    {
        uint8_t header_offset = *p++;
        uint8_t bit_count = *p++;
        update_state_bit(header[header_offset], bit_count);
    }

    STATE_REG &= ~0x40;
    STATE_REG &= ~0x20;
    (*(volatile uint8_t *)(0xFFF906)) = 0x70;
}
#endif

void swap_bytes(uint8_t *param1, uint8_t *param2)
{
    uint8_t temp = *param1;
    *param1 = *param2;
    *param2 = temp;
}

void send_byte_to_host(uint8_t data)
{
    while ((SCSR & 0x0100) == 0)
        ;
    SCDR = data;
}

void control_led_and_send(uint8_t value)
{
    send_byte_to_host(0x1b);
    send_byte_to_host('0' + value);

    uint32_t count = 0;
    uint8_t toggle = 0;
    uint8_t step = 0;

    while (step < (value * 2))
    {
        count++;
        if (count > 0x3a98)
        {
            toggle ^= 1;
            count = 0;
            step++;
        }

        if (toggle)
        {
            if (SCSR & 0x0100)
                SCDR = 0xff;
            OPR_SET = 0x60;
        }
        else
        {
            OPR_CLR = 0x60;
        }
    }
}

uint8_t get_char_with_timeout(uint8_t source, uint32_t *timeout_counter)
{
    uint32_t timeout = 0;
    while (timeout < 0xf4240)
    {
        if (source == 3) // QSM
        {
            if (SCSR & 0x40)
                return SCDR;
        }
        else // DUART
        {
            if (SRA & 0x01)
                return RBRA;
        }
        timeout++;
    }
    if (timeout_counter)
        (*timeout_counter)++;
    return 0;
}

void rc4_ksa(uint8_t *state, uint8_t *key_table, uint8_t *i_ptr, uint8_t *j_ptr)
{
    *j_ptr = 0;
    *i_ptr = *j_ptr;

#if defined(UHG_KSA)
    // VERSION_UHG: no NVRAM, no USP. Classic RC4 KSA keyed solely by the
    // 64-byte static ENC_TABLE (from table_old.h), cycled mod 64. The
    // second arg is unused in this variant.
    (void)key_table;

    for (int i = 0; i < 256; i++)
    {
        state[i] = (uint8_t)i;
    }

    uint8_t j = 0;
    for (int i = 0; i < 256; i++)
    {
        j = (j + state[i] + ENC_TABLE[i & 0x3f]) & 0xff;
        swap_bytes(&state[i], &state[j]);
    }
#else
    uint32_t usp_val;
    __asm__ volatile("move %%usp, %0" : "=a"(usp_val));

#ifdef KSA_USP_SCRAMBLE
    uint32_t scrambled_usp = usp_val ^ KSA_USP_SCRAMBLE;
#else
    uint32_t scrambled_usp = usp_val;
#endif
    uint8_t *usp_bytes = (uint8_t *)&scrambled_usp;

    uint8_t *nvram_data = (uint8_t *)0x1000;

    for (int i = 0; i < 256; i++)
    {
        state[i] = (uint8_t)i;
        uint8_t val = nvram_data[i];
        val ^= usp_bytes[i & 3];
#ifdef KSA_MIX_NOT_I
        val ^= (uint8_t)~i;
#endif
#ifdef KSA_MIX_I
        val ^= (uint8_t)i;
#endif
#ifdef KSA_MIX_I_SHIFT
        val ^= (uint8_t)(i << KSA_MIX_I_SHIFT);
#endif
        key_table[i] = val;
    }

    uint8_t j = 0;
    for (int i = 0; i < 256; i++)
    {
        j = (j + state[i] + key_table[i]) & 0xff;
        swap_bytes(&state[i], &state[j]);
    }
#endif
}

uint8_t rc4_crypt_byte(uint32_t data_byte, uint8_t *state, uint8_t *i_ptr, uint8_t *j_ptr)
{
#ifdef HAS_RC4_NMIX
    // Characteristic environment check
    volatile uint32_t *qsm_nmix = (volatile uint32_t *)0xfffc1a;

    if (*qsm_nmix != 0x4e4d4958) // "NMIX"
    {
        // Pointer obfuscation if NMIX is missing
        uint32_t p1 = (uint32_t)j_ptr;
        uint32_t p2 = (uint32_t)i_ptr;
        uint32_t p3 = (uint32_t)state;

        j_ptr = (uint8_t *)((p1 & 0xffff0000) | (p2 >> 16));
        i_ptr = (uint8_t *)((p2 & 0xffff0000) | ((uint32_t)state >> 16));
        state = (uint8_t *)((p3 & 0xffff0000) | (data_byte >> 16));
    }
#endif

    (*i_ptr)++;
    *j_ptr = (*j_ptr + state[*i_ptr]) & 0xff;

    swap_bytes(&state[*i_ptr], &state[*j_ptr]);

    uint8_t sum = (state[*i_ptr] + state[*j_ptr]) & 0xff;
    uint8_t K = state[sum];

    return (uint8_t)data_byte ^ K;
}

void process_communication(void)
{
    uint8_t source;
    uint8_t c;
    uint8_t rc4_state[256];
    uint8_t key_table[256];
    uint8_t i_idx = 0;
    uint8_t j_idx = 0;

    // -------------------------------------------------------------------------
    // Phase 1: Wait for magic trigger byte and verify magic sequence.
    //
    // The tables store (expected_protocol_byte - 1). The original comparison is:
    //   received == table[k] + 1
    // Both MAGIC1 and MAGIC2 sequences are 15 bytes long (D6 counts 1..15).
    //
    // MAGIC2 contains a trap: if bytes 0-6 all match, the loader treats this as
    // a hostile probe, clears ROM address 0x400, calls reset_and_wait(1), then
    // loops forever. MAGIC2 can therefore never complete successfully.
    // -------------------------------------------------------------------------
    while (1)
    {
        // Wait for trigger byte from either port
        while (1)
        {
            if (SCSR & 0x40)
            {
                c = SCDR;
                source = 3;
                break;
            }
            if (SRA & 0x01)
            {
                c = RBRA;
                source = 1;
                break;
            }
        }

        const uint8_t *magic;
        bool magic_is_type2;

        if (c == 0x1b)
        {
            magic = MAGIC1;
            magic_is_type2 = false;
        }
        else if (c == 0x7c)
        {
            magic = MAGIC2;
            magic_is_type2 = true;
        }
        else
        {
            continue; // restart
        }

        // Compare 15 follow-on bytes. Each expected received byte = magic[k] + 1.
        bool match = true;
        for (int k = 0; k < 15; k++)
        {
            // Receive next byte with inline timeout (original uses inline D5 counter,
            // resetting to 0 on timeout and retrying the same position).
            uint32_t timeout = 0;
            while (1)
            {
                if (source == 3)
                {
                    if (SCSR & 0x40) { c = SCDR; break; }
                }
                else
                {
                    if (SRA & 0x01) { c = RBRA; break; }
                }
                if (++timeout > 0xf4240)
                {
                    reset_and_wait(1);
                    timeout = 0;
                }
            }

            // FIX: comparison uses +1 to match table encoding
            if (c != (uint8_t)(magic[k] + 1))
            {
                match = false;
                break;
            }

            // FIX: MAGIC2 trap — after 7 successful bytes, signal error and
            // loop forever. MAGIC2 can never proceed to header reading.
            if (magic_is_type2 && k == 6)
            {
                *(volatile uint32_t *)0x400 = 0;
                reset_and_wait(1);
                while (1)
                    ;
            }
        }

        if (match)
            break;
    }

    // -------------------------------------------------------------------------
    // Phase 2: Receive 8-byte header, verify checksum, conditionally call
    //          process_header.
    //
    // The OR of all 8 header bytes is tested: if zero (all-zero header),
    // process_header is skipped entirely. This matches the original's
    //   tst.b (or_flag, A6) / beq LAB_000008ec
    // -------------------------------------------------------------------------
    uint8_t header[8];
    uint8_t checksum = 0;
    uint8_t header_or = 0;

    for (int k = 0; k < 8; k++)
    {
        uint32_t timeout = 0;
        header[k] = get_char_with_timeout(source, &timeout);
        if (timeout)
        {
            reset_and_wait(1);
            return;
        }
        if (k < 7)
            checksum += header[k];
        header_or |= header[k];
    }

    if (checksum != header[7])
    {
        reset_and_wait(1);
        return;
    }

#ifndef OLD_FAMILY
    // only call process_header if at least one header byte is non-zero
    if (header_or)
    {
        process_header(header);
    }
#else
    (void)header_or;
#endif

    // -------------------------------------------------------------------------
    // Phase 3: Receive 0x100 bytes PLAINTEXT into NVRAM at 0x1000–0x10ff.
    //          These bytes contain the end-address, checksums, and other
    //          metadata that check_magic validates. The original receives them
    //          raw (no encryption) before setting up RC4.
    // -------------------------------------------------------------------------
    uint8_t *plain_dest = (uint8_t *)0x1000;
    uint32_t timeout;

    if (source == 3) // QSM
    {
        while (plain_dest < (uint8_t *)0x1100)
        {
            uint8_t scsr_val;
            timeout = 0;
            while (1)
            {
                scsr_val = (uint8_t)SCSR;
                if (scsr_val & 0x40) break;
                if (++timeout > 0xf4240)
                {
                    reset_and_wait(1);
                    timeout = 0;
                }
            }
            c = SCDR;
            *plain_dest++ = c;
            if ((scsr_val & 0x0f) != 0)
            {
                reset_and_wait(1);
                return;
            }
        }
    }
    else // DUART
    {
        while (plain_dest < (uint8_t *)0x1100)
        {
            timeout = 0;
            while (!(SRA & 0x01))
            {
                if (++timeout > 0xf4240)
                {
                    reset_and_wait(1);
                    timeout = 0;
                }
            }
            *plain_dest++ = RBRA;
        }
    }

    // Validate the NVRAM block we just received
    if (!check_magic())
    {
        reset_and_wait(1);
        return;
    }

    // -------------------------------------------------------------------------
    // Phase 4: Set up RC4, then receive and decrypt the application image
    //          into memory starting at 0x1100.
    // -------------------------------------------------------------------------
    SYNCR = 0xa205; // matches SYNCR write at 0x984 in original
    SCCR0 = 0x05;

    rc4_ksa(rc4_state, key_table, &i_idx, &j_idx);

#ifdef HAS_RC4_NMIX
    // Set NMIX signature AFTER KSA (rc4_crypt_byte checks for it)
    (*(volatile uint32_t *)(0xFFFC1A)) = 0x4e4d4958; // "NMIX"
#endif

    uint8_t *app_dest = (uint8_t *)0x1100;

    if (source == 3) // QSM
    {
        while (app_dest < (uint8_t *)(*(volatile uint32_t *)0x1004) &&
               app_dest < (uint8_t *)0x400000) // 0x400000 safety bound
        {
            uint8_t scsr_val;
            timeout = 0;
            while (1)
            {
                scsr_val = (uint8_t)SCSR;
                if (scsr_val & 0x40) break;
                if (++timeout > 0xf4240)
                {
                    reset_and_wait(1);
                    return;
                }
            }
            c = SCDR;
            *app_dest++ = rc4_crypt_byte(c, rc4_state, &i_idx, &j_idx);

            if ((scsr_val & 0x0f) != 0)
            {
                reset_and_wait(1);
                return;
            }
        }
    }
    else // DUART
    {
        while (app_dest < (uint8_t *)(*(volatile uint32_t *)0x1004) &&
               app_dest < (uint8_t *)0x400000) // 0x400000 safety bound
        {
            timeout = 0;
            while (!(SRA & 0x01))
            {
                if (++timeout > 0xf4240)
                {
                    reset_and_wait(1);
                    return;
                }
            }
            c = RBRA;
            *app_dest++ = rc4_crypt_byte(c, rc4_state, &i_idx, &j_idx);
        }
    }
}

void reset_and_wait(uint32_t led_pattern)
{
    *(volatile uint32_t *)0x1004 = 0;
    *(volatile uint32_t *)0x100c = 0;
    *(volatile uint32_t *)0x104c = 0;

    uint32_t count = 0;
    while (count < 0x186a0)
    {
        if (SCSR & 0x40)
        {
            (void)SCDR;
            count = 0;
        }
        else if (SRA & 0x01)
        {
            (void)RBRA;
            count = 0;
        }
        else
            count++;
    }

    control_led_and_send((uint8_t)led_pattern);
    __asm__ volatile("bgnd; .word 0x60fe");
}

bool check_magic(void)
{
    uint32_t stack_base;
    __asm__ volatile ("move.l 0, %0" : "=d"(stack_base));

    if (~(*(volatile uint32_t *)0x1008) != *(volatile uint32_t *)0x1004)
        return false;
    if (~(*(volatile uint32_t *)0x1050) != *(volatile uint32_t *)0x104c)
        return false;

    if (stack_base <= *(volatile uint32_t *)0x1004)
        return false;
    if (stack_base <= *(volatile uint32_t *)0x1010)
        return false;
    if (stack_base <= *(volatile uint32_t *)0x1014)
        return false;

    if ((*(volatile uint32_t *)0x100c & 0xffffff00) != HEADER_MAGIC)
        return false;

    return true;
}

uint8_t verify_checksum(void)
{
    if (!check_magic())
        return 0;

#ifdef HAS_EXTRA_SIG_SCAN
    // VERSION_30ALT: completely different post-check_magic validation.
    // It reads a pointer from NVRAM[0x1058] into the just-loaded app, scans
    // forward (word-wise) for the marker 0x4afa4afa (double BGND), summing
    // the words until found or up to 0x186a0 iterations. Then adds a fixed
    // constant 0x95f4 and compares against the long stored at (end_addr - 3).
    //
    // Offsets and the -3 are taken directly from the original disassembly at
    // 0xb60..0xbca. Reverse-engineered behavior — verify against ROM before
    // trusting for production use.
    uint32_t stack_base = *(volatile uint32_t *)0x0000;
    uint32_t sig_ptr = *(volatile uint32_t *)0x1058;

    if (stack_base <= sig_ptr) return 0;
    if (sig_ptr < 0x1500)      return 0;

    uint32_t sum = 0;
    volatile uint16_t *p = (volatile uint16_t *)sig_ptr;
    for (uint32_t i = 0; i < 0x186a0; i++)
    {
        if (p[0] == 0x4afa && p[1] == 0x4afa) break;
        sum += (uint32_t)p[0];
        p++;
    }
    sum += 0x95f4;

    uint8_t *end = (uint8_t *)(*(volatile uint32_t *)0x1004);
    if ((uint32_t)end > 0x400000) return 0;

    uint32_t expected = *(volatile uint32_t *)(end - 3);
    return (sum == expected) ? 1 : 0;
#else
    uint8_t *start = (uint8_t *)0x1000;
    uint8_t *end = (uint8_t *)(*(volatile uint32_t *)0x1004);

    if ((uint32_t)end > 0x400000)
        return 0;

    uint32_t sum = 0;
    for (uint8_t *p = start + 4; p <= end; p++)
    {
        sum += *p;
    }

    if (sum != *(volatile uint32_t *)0x1000)
        return 0;
    return 1;
#endif
}

void init_duart(uint32_t slow_mode)
{
    OPCR = 0x00;
    ACR = 0xf0;
    CRA = 0x3a;
    CRA = 0x2a;
    MR1A = 0x13;
    MR1A = 0x07;

    if (!slow_mode)
    {
        CSRA = 0xcc;
        CRA = 0x90;
        CRA = 0xb0;
    }
    else
    {
        CSRA = 0x88;
        CRA = 0x80;
        CRA = 0xa0;
    }
    CRA = 0x1a;
    CRA = 0x45;
}

void main_task(void)
{
    init_duart(0);

    uint32_t d7_val;
    __asm__ volatile("move.l %%d2, %0" : "=d"(d7_val));

#ifdef HAS_WARM_BOOT
    // 5.0b variants check a cookie in D7 on entry. When it matches, verify the
    // NVRAM image and jump straight to the already-loaded application rather
    // than re-receiving it over the serial link.
    if (d7_val == 0x5f72d920)
    {
        if (verify_checksum())
        {
            control_led_and_send(3);
            uint32_t vbr_val = 0x1100;
            __asm__ volatile("movec %0, %%vbr" : : "d"(vbr_val));

            void (*app_entry)(void) = (void (*)(void))(*(volatile uint32_t *)0x104c);
            app_entry();
            while (1)
                ;
        }
    }
#endif

    control_led_and_send(2);

    if (d7_val == 0x100)
    {
        init_duart(1);
    }

    process_communication();

    SYNCR = 0x7f05;
    SCCR0 = 0x09;

    if (verify_checksum())
    {
        __asm__ volatile("bgnd; .word 0x60fe");
    }

    reset_and_wait(1);
}

__attribute__((optimize("omit-frame-pointer"))) int main(void)
{
    SIMCR = 0x40c5;
    SYPCR = 0x4c;
    SYNCR = 0x7f05;
    CSORBT = 0x6c70;

    __asm__ volatile("movea.l %%d3, %%a0; move %%a0, %%usp" : : : "a0", "memory");

    uint32_t stack_from_0;
    __asm__ volatile ("move.l 0, %0" : "=d"(stack_from_0));
#ifndef OLD_FAMILY
    uint32_t vector_4;
    __asm__ volatile ("move.l 4, %0" : "=d"(vector_4));
#endif

    uint32_t d0_val = stack_from_0;

#if defined(OLD_FAMILY)
    // ROTE/UHG: no NVRAM-based stack sizing; reserve a small guard below the
    // top of RAM and use that as the stack.
    d0_val -= 0x40;
#elif defined(STACK_DETECT_100C)
    // 30ALT: a recognized image magic at NVRAM 0x100c forces a fixed 0x80000
    // stack window; otherwise fall back to the top-of-RAM value at 0x0000.
    uint32_t magic_100c = *(volatile uint32_t *)0x100c;
    if (magic_100c == 0x31415926 || magic_100c == 0x61647001)
        d0_val = 0x80000;
    d0_val -= 0x80;
#else
    // 5.0a/5.0b: size the stack from the NVRAM byte at 0x100f (0x40000 << n).
    uint8_t nvram_100f = (*(volatile uint8_t *)(0x000100f));
    if (nvram_100f != 0)
    {
        d0_val = 0x40000;
        if (nvram_100f == 0x26)
            nvram_100f = 1;
        d0_val <<= (nvram_100f & 0x3f);
    }
    d0_val -= 0x80;
#endif
    __asm__ volatile("movea.l %0, %%sp" : : "d"(d0_val) : "memory");

#ifndef OLD_FAMILY
    // ROTE/UHG always program the chip selects; the 5.0/30ALT families skip
    // them when vector 4 reads back as all-ones (uninitialized boot ROM).
    if (vector_4 != 0xFFFFFFFF)
#endif
    {
        if (stack_from_0 == 0x80000)
        {
            CSBARBT = 0x05;
            CSPAR0 = 0x3fff;
            CSPAR1 = 0x03ff;
            CSBAR0 = 0x0405;
            CSBAR1 = 0x0405;
            CSBAR2 = 0x0405;
            CSBAR3 = 0x0005;
            CSBAR4 = 0x0005;
        }
        else
        {
            CSBARBT = 0x07;
            CSPAR0 = 0x3fff;
            CSPAR1 = 0x03fd;
            CSBAR0 = 0x1007;
            CSBAR1 = 0x1007;
            CSBAR2 = 0x1007;
            CSBAR3 = 0x0007;
            CSBAR4 = 0x0007;
        }
        CSOR0 = 0x5470;
        CSOR1 = 0x3470;
        CSOR2 = 0x6c70;
        CSOR3 = 0x5470;
        CSOR4 = 0x3470;
    }

    CSBAR5 = 0xfff8;
    CSBAR6 = 0x8000;
    CSBAR8 = 0x8000;
    CSBAR9 = 0x8000;
    CSBAR10 = 0x8000;
    CSOR5 = 0x2bca;
    CSOR6 = 0x2fc4;

#ifdef CS7_UNCONDITIONAL
    CSBAR7 = 0x8000;
    CSOR7 = 0x2fc6;
#else
    if (stack_from_0 != 0x200000)
    {
        CSBAR7 = 0x8000;
        CSOR7 = 0x2fc6;
    }
#endif

    CSOR8 = 0x2ff0;
    CSOR9 = 0x7bf0;
    CSOR10 = 0x37f0;
    PORTE0 = 0x00;
    PORTE1 = 0x00;
    PORTF0 = 0x00;
    PORTF1 = 0x00;
    DDRE = 0xf8;
    DDRF = 0x00;
    PEPAR = 0xff;
    PFPAR = 0x60;
    PICR = 0xf;
    PITR = 0x00;
    QSMCR = 0x8a;
    QILR = 0x0550;
    PORTQS = 0x00;
    PQSPAR = 0x82;

    CR0 = 0x00;
    CR1 = 0x00;
    CR2 = 0x00;
    CR3 = 0x00;
    CR4 = 0x00;
    CR5 = 0x00;
    CR6 = 0x00;
    CR7 = 0x00;

    SPCR0 = 0x0104;
    SPCR1 = 0x0404;
    SPCR2 = 0x00;
    SPCR3 = 0x00;
    SCCR0 = 0x09;
    SCCR1 = 0x2c;

    GPTMCR = 0x80;
    ICR = 0x00;
    TOC1 = 0xFFFF;
    TOC2 = 0xFFFF;
    TOC3 = 0xFFFF;
    TOC4 = 0xFFFF;
    TI4_O5 = 0xFFFF;
    OC1 = 0x00;
    TCTL = 0x00;
#ifdef EXTRA_FFF9_REGS
    // 30ALT/ROTE/UHG additionally clear these timer/port registers.
    PAC = 0x00;
    CFORC_PWMC = 0x00;
    PWM = 0x00;
#endif
    GP = 0x7000;
#ifdef EXTRA_FFF9_REGS
    TMSK = 0x00;
#endif

    main_task();

    return 0;
}

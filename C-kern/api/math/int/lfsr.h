/* title: Linear-feedback-shift-register

   Used for generating pseudo-random numbers,
   for a description see:
   - Algebraic Structures in Cryptography by V. Niemi
   - Wikipedia (https://en.wikipedia.org/wiki/Linear-feedback_shift_register#Galois_LFSRs)
   - Tutorial on Linear Feedback Shift Registers by EETimes (https://www.eetimes.com/document.asp?doc_id=1274550)
   - Encyclopedia of Cryptography and Security by Anne Canteout (https://www.rocq.inria.fr/secret/Anne.Canteaut/MPRI/chapter3.pdf)

   In German:
   Linear rückgekoppeltes Schieberegister - siehe auch https://de.wikipedia.org/wiki/LFSR.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2018 Jörg Seebohn

   file: C-kern/api/math/int/lfsr.h
    Header file <Linear-feedback-shift-register>.

   file: C-kern/math/int/lfsr.c
    Implementation file <Linear-feedback-shift-register impl>.
*/
#ifndef CKERN_MATH_INT_LFSR_HEADER
#define CKERN_MATH_INT_LFSR_HEADER

// === exported types
struct lfsr_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_lfsr
 * Test <lfsr_t> functionality. */
int unittest_math_int_lfsr(void);
#endif


/* struct: lfsr_t
 * Implements linear-feeback shift registers as Galois LFSR.
 * The register has length n. The value n could be chosen to be
 * from 2 to 64. The current state determines the register value.
 * At every step the bits are shifted right.
 * The content of the least significant bit (0x1)
 * is shifted into the highest bit position.
 * Either a bit value is determined by its left neigbour only
 * (or lsb in case of highest bit). Or by its left neighbour
 * value xored together with value of the shifted out lsb.
 *
 * The highest set tap bit determines the length of the register.
 * The value 0x8000(==tapbits) configures a 16bit register.
 * Additional set bits 0x3400 (0xb400==tapbits) determine
 * which bits are additionally xored with the shifted out lsb bit.
 *
 * Example 16 bit (hardware) register:
 * The tapbits are 0xa000.
 * >    -----------[ bit positions ]----------
 * >    16       14  12  10   8   6   4   2 1
 * >    -----------[ register state ]---------
 * > ┌> |1|0|-X->|0|1|0|1|1|0|1|0|1|1|0|0|0|1|──┐
 * > │        │                                 │
 * > └────────┴─────────────────────────────────┘
 * X denotes a xor gate in the figure.
 * At every clock cycle the bits are shifted right.
 * The lsb bit is shifted into position 16 (tapbit:0x8000).
 * Bit 14 is calculated as "bit14 = bit15 ^ lsb" (tapbit:0x2000).
 *
 * Special state 0:
 * A state of 0 locks the register into this state.
 * Every next step produces also 0 as a result.
 *
 * */
typedef struct lfsr_t {
   uint64_t state;
   uint64_t tapbits;
} lfsr_t;

// group: lifetime

/* define: lfsr_INIT
 * Static initializer. */
#define lfsr_INIT(state, tapbits) \
         { state, tapbits }

/* define: lfsr_FREE
 * Static initializer. */
#define lfsr_FREE \
         { 0, 0 }

/* function: init_lfsr
 * Sets state and tapbits of lfsr.
 * The MSB bit determines the size of the register.
 * Other bits determine which bit positions are additionally
 * xored the shifted out lsb value after the shift. */
void init_lfsr(/*out*/lfsr_t *lfsr, uint64_t state, uint64_t tapbits);

// group: query

/* function: state_lfsr
 * Returns the current register value (state). */
uint64_t state_lfsr(const lfsr_t *lfsr);

// group: update

/* function: reset_lfsr
 * Returns the current register value (state). */
void reset_lfsr(lfsr_t *lfsr, uint64_t state);

/* function: next_lfsr
 * Shifts the register right and xors the tapbits with the value of the shifted out lsb bit.
 * The new register value (state) is returned. */
uint64_t next_lfsr(lfsr_t *lfsr);


// section: inline implementation

/* define: state_lfsr
 * Implements <lfsr_t.state_lfsr>. */
#define state_lfsr(lfsr) \
         ((lfsr)->state)

/* define: next_lfsr
 * Implements <lfsr_t.next_lfsr>. */
#define next_lfsr(lfsr) \
         ( __extension__ ({                           \
            lfsr_t * _o = (lfsr);                     \
            uint64_t _state = _o->state;              \
            unsigned _bit = (unsigned) (_state & 1u); \
            _state >>= 1;                             \
            if (_bit)                                 \
               _state ^= _o->tapbits;                 \
            _o->state = _state;                       \
            _state;                                   \
         }))

/* define: reset_lfsr
 * Implements <lfsr_t.reset_lfsr>. */
#define reset_lfsr(lfsr, _state) \
         ((void)((lfsr)->state=(_state)))



#endif

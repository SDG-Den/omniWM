/*
 * omni_layout.h - on-disk ABI of the shared-memory config store.
 *
 * The ABI was frozen when this file was generated. The freeze was lifted on
 *  2026-09-26, while the compositor subsystems are still at design stage, so
 *  that a change to the tag table, a composite payload, the section set, or a
 *  capacity constant costs a paragraph rather than a compatibility story. See
 * devnotes/layoutengine.md section 2.9 for why, and section 2.10 for the list
 * of changes the layout engine asked for.
 *
 * That list has now been applied. Four changes, all of them consequences of
 * devnotes/layoutengine.md sections 2.10 and 4.6:
 *
 *   1. OMNI_TAG_CONSTRAINT (0x32), a fixed-record constraint program, so a
 *      layout slot holds a value rather than a positional tuple.
 *   2. OMNI_TAG_CLIENT_RULE (0x33), because OMNI_TAG_RULE matches keys and
 *      replaces values, and every rule the layout set needs matches a client
 *      and changes membership instead.
 *   3. OMNI_TAG_BINDING is now framed and carries an argument list, because a
 *      snap-by-keybind takes a direction and wm.cycle_layout already takes a
 *      layout name in ipc.md's own example.
 *   4. OMNI_SECTION_SOLVED_LAYOUT, a non-journalled section holding the most
 *      recent solve, so an external program can read a layout and a solve does
 *      not cost a commit.
 *
 * OMNI_FORMAT_VERSION stays 1. The compositor does not exist yet and the first
 * runnable build is v1, so these are changes to the only format there has ever
 * been, not changes to a released one.
 *
 * AI-SCAFFOLDED FILE. Generated as part of the architecture-audit
 * remediation pass, not written from working code. Every value here was
 * transcribed from devnotes/configstorelayout.md and cross-checked against
 * devnotes/configstorage.md; both documents remain the prose source of truth
 * and this header is their only numeric home. If the two disagree, the
 * documents are wrong and this file is the bug.
 *
 * Normative references:
 *   devnotes/configstorelayout.md  sections 2-10 (byte layout, commit protocol)
 *   devnotes/configstorage.md      sections 4, 5, 8, 12 (values, guards, requests)
 *
 * Design rules this file obeys, and why:
 *
 *  1. No struct mirrors the block. Every field is an offset constant plus an
 *     explicit width. The block format is native-endian and platform-native in
 *     size, so a struct's compiler-chosen padding would silently become the
 *     ABI on any host where the natural layout differs. Numbers cannot drift.
 *  2. No macro computes an offset from a previous offset. The derivation in
 *     configstorelayout.md section 2 is a formula chain; encoding it as C would
 *     let the compiler constant-fold a value that then diverges from the
 *     documented one. Offsets are literals; the formulas are asserted below.
 *  3. Every derived geometry value is a compile-time assert, not a comment, so
 *     the arithmetic is checkable at build time and cannot rot.
 *  4. Bit flags are named masks carrying their bit position, so a mistyped
 *     shift is a compile error rather than a wrong mask.
 *  5. Nothing here hides a bounds check. The guard tiers in configstorage.md
 *     section 12 are the consumer's job and stay visible in the consumer.
 *
 * Endianness: all multi-byte block fields are native-endian, tied to
 * OMNI_FORMAT_VERSION. There is no byte swapping and no cross-endian support
 * at v1. A block is only ever read by a host of the same endianness.
 */

#ifndef OMNIWM_SHARED_OMNI_LAYOUT_H
#define OMNIWM_SHARED_OMNI_LAYOUT_H

#include <stdint.h>

/* The project should build with C11 or later, but this header must not be the
 * thing that breaks a build over it. Prefer the real keyword, fall back to the
 * negative-array-size trick on pre-C11 compilers. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define OMNI_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#else
#define OMNI_STATIC_ASSERT(condition, message) \
	typedef char omni_static_assert_##__LINE__[(condition) ? 1 : -1]
#endif

/* ------------------------------------------------------------------ */
/* 0. Derivation helper                                                */
/* ------------------------------------------------------------------ */

/* Rounds up to a power-of-two multiple. Used only inside static asserts. */
#define OMNI_ALIGN_UP(value, alignment) \
	(((value) + (alignment) - UINT32_C(1)) & ~((alignment) - UINT32_C(1)))

/* ------------------------------------------------------------------ */
/* 1. Block identity and limits (configstorelayout.md section 2)       */
/* ------------------------------------------------------------------ */

#define OMNI_MAGIC UINT32_C(0x4F574D42) /* "OWMB" in memory-writing order */
#define OMNI_FORMAT_VERSION UINT32_C(1) /* v1 is the first runnable build     */

/* Alignment for sections, pool allocations, and atomic access. */
#define OMNI_ALIGN UINT32_C(16)

#define OMNI_BLOCK_INITIAL_SIZE (UINT32_C(4) * 1024 * 1024)
#define OMNI_BLOCK_MAX_SIZE (UINT32_C(512) * 1024 * 1024)
#define OMNI_POOL_GROWTH UINT32_C(2) /* doubling factor on pool growth    */

/* ------------------------------------------------------------------ */
/* 2. Fixed geometry                                                   */
/* ------------------------------------------------------------------ */

#define OMNI_HEADER_SIZE UINT32_C(0x300)
#define OMNI_SECTION_TABLE_OFFSET UINT32_C(0x100)
#define OMNI_SECTION_TABLE_SIZE UINT32_C(0x200)
#define OMNI_SECTION_SLOT_COUNT UINT32_C(16)
#define OMNI_SECTION_SLOT_SIZE UINT32_C(32)
#define OMNI_FIRST_SECTION_OFF UINT32_C(0x1000)

#define OMNI_CATALOG_OFF UINT32_C(0x1000)
#define OMNI_CATALOG_SLOT_COUNT UINT32_C(16384)
#define OMNI_CATALOG_ENTRY_SIZE UINT32_C(32)
#define OMNI_ENTRY_NAME_MAX UINT32_C(4095) /* bytes including the NUL */

#define OMNI_JOURNAL_OFF UINT32_C(0x81000)
#define OMNI_JOURNAL_HEADER_SIZE UINT32_C(32)
#define OMNI_JOURNAL_CAPACITY UINT32_C(4096)
#define OMNI_JOURNAL_SLOT_SIZE UINT32_C(64)
#define OMNI_JOURNAL_INLINE_LIMIT UINT32_C(8) /* values <= 8 B go inline */

#define OMNI_REQUESTS_OFF UINT32_C(0xC2000)
#define OMNI_REQUEST_SLOT_COUNT UINT32_C(256)
#define OMNI_REQUEST_SLOT_SIZE UINT32_C(0x1100)
#define OMNI_REQUEST_NAME_MAX UINT32_C(4095)  /* bytes including the NUL */
#define OMNI_REQUEST_VALUE_MAX UINT32_C(128)  /* inline initial-value cap */
/* Terminal-slot reclaim deadline. One definition; only the WM reads it. */
#define OMNI_REQUEST_RECLAIM_MS UINT32_C(5000)

#define OMNI_REGION_DESC_OFF UINT32_C(0x1D2000)
#define OMNI_REGION_DESC_SIZE UINT32_C(32)
#define OMNI_REGION_DESC_COUNT UINT32_C(64)
#define OMNI_REGION_SLOT_COUNT_MAX UINT32_C(2) /* double buffering is 1 or 2 */
#define OMNI_REGION_FRAME_MAX_BYTES (UINT32_C(34) * 1024 * 1024)
#define OMNI_REGION_TOTAL_MAX_BYTES (UINT32_C(68) * 1024 * 1024)

/* Solved layout section. Fixed, single-buffered, never journalled and never
 * saved: it holds the geometry the most recent solve produced, so a script can
 * read a layout, and it is overwritten rather than appended on every pass. */
#define OMNI_SOLVED_OFF UINT32_C(0x1D2800)
#define OMNI_SOLVED_SECTION_SIZE UINT32_C(0x8000)
#define OMNI_SOLVED_HEADER_SIZE UINT32_C(16)
#define OMNI_SOLVED_NODE_SIZE UINT32_C(24)
#define OMNI_SOLVED_SLOT_COUNT_MAX UINT32_C(1024)
/* Bytes the header plus a full node array occupies. Must fit the section. */
#define OMNI_SOLVED_BYTES \
	(OMNI_SOLVED_HEADER_SIZE + OMNI_SOLVED_NODE_SIZE * OMNI_SOLVED_SLOT_COUNT_MAX)

#define OMNI_POOL_OFF UINT32_C(0x1DB000) /* == FIXED_END, see section 3 */

/* Sentinel stored in place of a pool offset or slot index meaning "none". */
#define OMNI_REF_NONE UINT32_C(0xFFFFFFFF)

/* ------------------------------------------------------------------ */
/* 3. Derived geometry, asserted at build time                         */
/* ------------------------------------------------------------------ */

#define OMNI_CATALOG_SIZE (OMNI_CATALOG_SLOT_COUNT * OMNI_CATALOG_ENTRY_SIZE)
#define OMNI_JOURNAL_SIZE \
	(OMNI_JOURNAL_HEADER_SIZE + OMNI_JOURNAL_CAPACITY * OMNI_JOURNAL_SLOT_SIZE)
#define OMNI_REQUESTS_SIZE (OMNI_REQUEST_SLOT_COUNT * OMNI_REQUEST_SLOT_SIZE)
#define OMNI_REGION_DESC_TOTAL (OMNI_REGION_DESC_COUNT * OMNI_REGION_DESC_SIZE)
#define OMNI_FIXED_END OMNI_POOL_OFF
#define OMNI_INITIAL_POOL (OMNI_BLOCK_INITIAL_SIZE - OMNI_FIXED_END)
#define OMNI_MAX_POOL (OMNI_BLOCK_MAX_SIZE - OMNI_FIXED_END)

OMNI_STATIC_ASSERT(OMNI_CATALOG_SIZE == UINT32_C(0x80000), "catalog size");
OMNI_STATIC_ASSERT(OMNI_JOURNAL_SIZE == UINT32_C(0x40020), "journal size");
OMNI_STATIC_ASSERT(OMNI_REQUESTS_SIZE == UINT32_C(0x110000), "request size");
OMNI_STATIC_ASSERT(OMNI_REGION_DESC_TOTAL == UINT32_C(0x800), "descriptor size");
OMNI_STATIC_ASSERT(OMNI_SOLVED_BYTES == UINT32_C(0x6010), "solved layout bytes");
OMNI_STATIC_ASSERT(OMNI_SOLVED_BYTES <= OMNI_SOLVED_SECTION_SIZE,
	"solved layout fits its section");
OMNI_STATIC_ASSERT(OMNI_SOLVED_OFF + OMNI_SOLVED_SECTION_SIZE <= OMNI_POOL_OFF,
	"solved layout section");

/* Section bases are derived, so assert the derivation rather than the number. */
OMNI_STATIC_ASSERT(OMNI_JOURNAL_OFF ==
                   OMNI_ALIGN_UP(OMNI_CATALOG_OFF + OMNI_CATALOG_SIZE, 0x1000),
	"journal offset");
OMNI_STATIC_ASSERT(OMNI_REQUESTS_OFF ==
                   OMNI_ALIGN_UP(OMNI_JOURNAL_OFF + OMNI_JOURNAL_SIZE, 0x1000),
	"requests offset");
OMNI_STATIC_ASSERT(OMNI_REGION_DESC_OFF == OMNI_REQUESTS_OFF + OMNI_REQUESTS_SIZE,
	"descriptor offset");
OMNI_STATIC_ASSERT(OMNI_SOLVED_OFF == OMNI_REGION_DESC_OFF + OMNI_REGION_DESC_TOTAL,
	"solved layout offset");
OMNI_STATIC_ASSERT(OMNI_POOL_OFF ==
                   OMNI_ALIGN_UP(OMNI_SOLVED_OFF + OMNI_SOLVED_SECTION_SIZE, 0x1000),
	"pool offset");

/* Every fixed section must end at or before the pool. */
OMNI_STATIC_ASSERT(OMNI_CATALOG_OFF + OMNI_CATALOG_SIZE <= OMNI_POOL_OFF, "catalog");
OMNI_STATIC_ASSERT(OMNI_JOURNAL_OFF + OMNI_JOURNAL_SIZE <= OMNI_POOL_OFF, "journal");
OMNI_STATIC_ASSERT(OMNI_REQUESTS_OFF + OMNI_REQUESTS_SIZE <= OMNI_POOL_OFF,
	"requests");
OMNI_STATIC_ASSERT(OMNI_REGION_DESC_OFF + OMNI_REGION_DESC_TOTAL <= OMNI_POOL_OFF,
	"descriptors");

/* Sizes, alignment, and headroom. */
OMNI_STATIC_ASSERT(OMNI_HEADER_SIZE <= OMNI_FIRST_SECTION_OFF, "header fits");
OMNI_STATIC_ASSERT(OMNI_SECTION_TABLE_OFFSET + OMNI_SECTION_TABLE_SIZE <=
                   OMNI_HEADER_SIZE,
	"section table inside header");
OMNI_STATIC_ASSERT(OMNI_SECTION_SLOT_COUNT * OMNI_SECTION_SLOT_SIZE ==
                   OMNI_SECTION_TABLE_SIZE,
	"section table size");
OMNI_STATIC_ASSERT((OMNI_ALIGN & (OMNI_ALIGN - 1)) == 0, "align is a power of two");
OMNI_STATIC_ASSERT(OMNI_CATALOG_OFF % 0x1000 == 0, "catalog page aligned");
OMNI_STATIC_ASSERT(OMNI_JOURNAL_OFF % 0x1000 == 0, "journal page aligned");
OMNI_STATIC_ASSERT(OMNI_REQUESTS_OFF % 0x1000 == 0, "requests page aligned");
OMNI_STATIC_ASSERT(OMNI_REGION_DESC_OFF % 0x1000 == 0, "descriptors page aligned");
OMNI_STATIC_ASSERT(OMNI_POOL_OFF % 0x1000 == 0, "pool page aligned");
OMNI_STATIC_ASSERT(OMNI_BLOCK_INITIAL_SIZE % 0x1000 == 0, "initial size pages");
OMNI_STATIC_ASSERT(OMNI_BLOCK_MAX_SIZE % 0x1000 == 0, "max size pages");
OMNI_STATIC_ASSERT(OMNI_BLOCK_INITIAL_SIZE <= OMNI_BLOCK_MAX_SIZE, "size order");
OMNI_STATIC_ASSERT(OMNI_INITIAL_POOL > 0, "initial pool non-empty");
OMNI_STATIC_ASSERT(OMNI_MAX_POOL > OMNI_INITIAL_POOL, "pool headroom");

/* ------------------------------------------------------------------ */
/* 4. Block header field offsets (configstorelayout.md section 3)     */
/* ------------------------------------------------------------------ */

#define OMNI_HDR_OFF_MAGIC UINT32_C(0)
#define OMNI_HDR_OFF_FORMAT_VERSION UINT32_C(4)
#define OMNI_HDR_OFF_BLOCK_SIZE UINT32_C(8)
#define OMNI_HDR_OFF_HEADER_SIZE UINT32_C(12)
#define OMNI_HDR_OFF_FUTEX UINT32_C(16)
#define OMNI_HDR_OFF_STATE UINT32_C(20)
#define OMNI_HDR_OFF_COMMIT_ID UINT32_C(24)
#define OMNI_HDR_OFF_EPOCH UINT32_C(32)
#define OMNI_HDR_OFF_BOOT_TIME_NS UINT32_C(40)
#define OMNI_HDR_OFF_CAPABILITIES UINT32_C(48)
#define OMNI_HDR_OFF_WM_PID UINT32_C(56)
#define OMNI_HDR_OFF_POOL_BASE UINT32_C(64)
#define OMNI_HDR_OFF_POOL_SIZE UINT32_C(72)
#define OMNI_HDR_OFF_ARENA_END UINT32_C(80)
#define OMNI_HDR_OFF_REGION_HEAD UINT32_C(88)
#define OMNI_HDR_OFF_CATALOG_FREE_HEAD UINT32_C(96)
#define OMNI_HDR_OFF_CATALOG_FREE_COUNT UINT32_C(100)
#define OMNI_HDR_OFF_REQUEST_TICKET_NEXT UINT32_C(104)
#define OMNI_HDR_OFF_REGION_DESC_COUNT UINT32_C(108)
#define OMNI_HDR_OFF_SECTION_TABLE_OFFSET UINT32_C(112)
#define OMNI_HDR_OFF_SECTION_TABLE_SIZE UINT32_C(116)
#define OMNI_HDR_OFF_FIRST_SECTION_OFFSET UINT32_C(120)
#define OMNI_HDR_OFF_REGION_DESC_BASE UINT32_C(128)
#define OMNI_HDR_OFF_WRITER_PID UINT32_C(136)
#define OMNI_HDR_OFF_WRITER_TOKEN UINT32_C(140)
#define OMNI_HDR_OFF_COMMIT_STATE UINT32_C(144)
#define OMNI_HDR_OFF_ACTIVE_COMMIT_ID UINT32_C(152)
#define OMNI_HDR_OFF_READY UINT32_C(160) /* u32, service readiness */
#define OMNI_HDR_RESERVED_TAIL_START UINT32_C(164) /* through offset 255 */

/* Header state. This axis is block validity only. */
#define OMNI_STATE_CREATING UINT8_C(0)
#define OMNI_STATE_READY UINT8_C(1)
#define OMNI_STATE_BROKEN UINT8_C(2) /* terminal for this epoch */

/* Readiness. Orthogonal to state: a block can be valid while the compositor is
 * still activating, and the two conditions have different client recovery. */
#define OMNI_READY_NOT_READY UINT32_C(0)
#define OMNI_READY_READY UINT32_C(1)
#define OMNI_READY_DEGRADED UINT32_C(2)
#define OMNI_READY_FAILED UINT32_C(3)

/* Commit state. */
#define OMNI_COMMIT_IDLE UINT32_C(0)
#define OMNI_COMMIT_ACTIVE UINT32_C(1)
#define OMNI_COMMIT_GROWING UINT32_C(2)
#define OMNI_COMMIT_BROKEN UINT32_C(3)

/* Section capability bits. */
#define OMNI_CAP_HAS_CATALOG (UINT64_C(1) << 0)
#define OMNI_CAP_HAS_ARENA (UINT64_C(1) << 1)
#define OMNI_CAP_HAS_JOURNAL (UINT64_C(1) << 2)
#define OMNI_CAP_HAS_REQUESTS (UINT64_C(1) << 3)
#define OMNI_CAP_HAS_REGION_DESC (UINT64_C(1) << 4)
#define OMNI_CAP_HAS_REGION_PAYLOAD (UINT64_C(1) << 5)
#define OMNI_CAP_HAS_SOCKET (UINT64_C(1) << 6)
#define OMNI_CAP_HAS_SOLVED_LAYOUT (UINT64_C(1) << 7)
#define OMNI_CAP_DEFAULT UINT64_C(0xFF)

/* Section ids; row order is fixed and id == row. */
#define OMNI_SECTION_NONE UINT32_C(0)
#define OMNI_SECTION_CATALOG UINT32_C(1)
#define OMNI_SECTION_ARENA UINT32_C(2)
#define OMNI_SECTION_JOURNAL UINT32_C(3)
#define OMNI_SECTION_REQUESTS UINT32_C(4)
#define OMNI_SECTION_REGION_DESC UINT32_C(5)
#define OMNI_SECTION_REGION_PAYLOAD UINT32_C(6)
#define OMNI_SECTION_SOLVED_LAYOUT UINT32_C(7)

/* Section table row field offsets. */
#define OMNI_SECTION_ROW_OFF_ID UINT32_C(0)
#define OMNI_SECTION_ROW_OFF_OFFSET UINT32_C(4)
#define OMNI_SECTION_ROW_OFF_SIZE UINT32_C(8)
#define OMNI_SECTION_ROW_OFF_FLAGS UINT32_C(16)
#define OMNI_SECTION_ROW_OFF_RESERVED UINT32_C(20)

/* Section row flags. Protocol markers, not ownership or access control.
 * A section without JOURNALLED is written by the producer alone and is
 * neither appended to the journal nor included by a save. That is the
 * solved layout section's whole reason for existing. */
#define OMNI_SECTION_FLAG_PRESENT_0 (UINT32_C(1) << 0)
#define OMNI_SECTION_FLAG_JOURNALLED_1 (UINT32_C(1) << 1)
#define OMNI_SECTION_FLAG_RESERVED_MASK UINT32_C(0xFFFFFFFC)

/* Fields accessed with aligned atomics, and their required alignment. */
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_FUTEX % 4 == 0, "futex aligned");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_COMMIT_STATE % 4 == 0, "commit_state aligned");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_ACTIVE_COMMIT_ID % 8 == 0, "active id aligned");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_READY % 4 == 0, "ready aligned");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_READY + 4 <= OMNI_HDR_RESERVED_TAIL_START,
	"ready fits in the reserved tail");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_COMMIT_ID % 8 == 0, "commit_id aligned");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_EPOCH % 8 == 0, "epoch aligned");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_POOL_BASE % 8 == 0, "pool_base aligned");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_POOL_SIZE % 8 == 0, "pool_size aligned");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_ARENA_END % 8 == 0, "arena_end aligned");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_REGION_HEAD % 8 == 0, "region_head aligned");

/* ------------------------------------------------------------------ */
/* 5. Arena frame header (configstorelayout.md section 5)             */
/* ------------------------------------------------------------------ */

#define OMNI_FRAME_OFF_LENGTH UINT32_C(0)  /* payload bytes, excl. header */
#define OMNI_FRAME_OFF_RESERVED32 UINT32_C(4)
#define OMNI_FRAME_OFF_RESERVED64 UINT32_C(8)
#define OMNI_FRAME_HEADER_SIZE UINT32_C(16)
#define OMNI_FRAME_OFF_PAYLOAD UINT32_C(16)

/* A framed allocation occupies FRAME_HEADER_SIZE + align16(payload_len). */
#define OMNI_FRAME_ALLOC_SIZE(payload_len) \
	(OMNI_FRAME_HEADER_SIZE + OMNI_ALIGN_UP((payload_len), OMNI_ALIGN))

/* ------------------------------------------------------------------ */
/* 6. Catalog entry (configstorelayout.md section 6)                   */
/* ------------------------------------------------------------------ */

#define OMNI_ENTRY_OFF_GENERATION UINT32_C(0)  /* u64, non-zero when live */
#define OMNI_ENTRY_OFF_NAME_REF UINT32_C(8)    /* u32 arena frame offset  */
#define OMNI_ENTRY_OFF_TYPE_TAG UINT32_C(12)   /* u16                     */
#define OMNI_ENTRY_OFF_FLAGS UINT32_C(14)      /* u16                     */
#define OMNI_ENTRY_OFF_LENGTH UINT32_C(16)     /* u32                     */
#define OMNI_ENTRY_OFF_BODY_REF UINT32_C(20)   /* u32                     */
#define OMNI_ENTRY_OFF_VALUE_INLINE UINT32_C(24) /* u64                   */

/* Entry flags. Bit 0 is reserved; it was MUTABLE, which no rule read.
 * Bit 3 was PERSIST, which was removed. Scope is expressed by exclusion:
 * an unmarked entry is ordinary configuration. */
#define OMNI_ENTRY_FLAG_DESTROYED_1 (UINT16_C(1) << 1)
#define OMNI_ENTRY_FLAG_FREE_2 (UINT16_C(1) << 2)
#define OMNI_ENTRY_FLAG_EPHEMERAL_3 (UINT16_C(1) << 3)
#define OMNI_ENTRY_FLAG_WINDOW_DEPENDENT_4 (UINT16_C(1) << 4)
#define OMNI_ENTRY_FLAG_RESERVED_MASK UINT16_C(0xFFE1)

/* A FREE entry reuses name_ref as the next freelist index. */
#define OMNI_FREELIST_END UINT32_C(0xFFFFFFFF)

/* ------------------------------------------------------------------ */
/* 7. Type tags (configstorage.md section 4)                          */
/* ------------------------------------------------------------------ */

#define OMNI_TAG_BOOL UINT16_C(0x01)
#define OMNI_TAG_I8 UINT16_C(0x02)
#define OMNI_TAG_I16 UINT16_C(0x03)
#define OMNI_TAG_I32 UINT16_C(0x04)
#define OMNI_TAG_I64 UINT16_C(0x05)
#define OMNI_TAG_U8 UINT16_C(0x06)
#define OMNI_TAG_U16 UINT16_C(0x07)
#define OMNI_TAG_U32 UINT16_C(0x08)
#define OMNI_TAG_U64 UINT16_C(0x09)
#define OMNI_TAG_F32 UINT16_C(0x0A)
#define OMNI_TAG_F64 UINT16_C(0x0B)
#define OMNI_TAG_CHAR UINT16_C(0x0C)
#define OMNI_TAG_STRING UINT16_C(0x0D)
#define OMNI_TAG_BLOB UINT16_C(0x0E)
#define OMNI_TAG_DURATION UINT16_C(0x0F)
#define OMNI_TAG_RATIO UINT16_C(0x10)
#define OMNI_TAG_PERCENT UINT16_C(0x11)
#define OMNI_TAG_KEYSYM UINT16_C(0x12)
#define OMNI_TAG_KEYCODE UINT16_C(0x13)
#define OMNI_TAG_MODMASK UINT16_C(0x14)
#define OMNI_TAG_POINT UINT16_C(0x15)
#define OMNI_TAG_SIZE UINT16_C(0x16)
#define OMNI_TAG_RECT UINT16_C(0x17)
#define OMNI_TAG_OFFSET UINT16_C(0x18)
#define OMNI_TAG_VEC2 UINT16_C(0x19)
#define OMNI_TAG_VEC3 UINT16_C(0x1A)
#define OMNI_TAG_VEC4 UINT16_C(0x1B)
#define OMNI_TAG_VEC2I UINT16_C(0x1C)
#define OMNI_TAG_VEC2U UINT16_C(0x1D)
#define OMNI_TAG_RGBA8 UINT16_C(0x1E)
#define OMNI_TAG_ARGB8 UINT16_C(0x1F)
#define OMNI_TAG_RGBA32F UINT16_C(0x20)
#define OMNI_TAG_HSVA8 UINT16_C(0x21)
#define OMNI_TAG_REGION_REF UINT16_C(0x22)
#define OMNI_TAG_ENTRY_REF UINT16_C(0x23)
#define OMNI_TAG_ENUM UINT16_C(0x24)
#define OMNI_TAG_OPTION UINT16_C(0x25)
#define OMNI_TAG_ARRAY UINT16_C(0x26)
#define OMNI_TAG_TUPLE UINT16_C(0x27)
#define OMNI_TAG_FONT_DESC UINT16_C(0x28)
#define OMNI_TAG_SHADER_REF UINT16_C(0x29)
#define OMNI_TAG_CURSOR_REF UINT16_C(0x2A)
#define OMNI_TAG_BINDING UINT16_C(0x2B)
#define OMNI_TAG_RULE UINT16_C(0x2C)
#define OMNI_TAG_GRADIENT UINT16_C(0x2D)
#define OMNI_TAG_PATH UINT16_C(0x2E)
#define OMNI_TAG_EXEC UINT16_C(0x2F)
#define OMNI_TAG_STATE UINT16_C(0x30)
#define OMNI_TAG_DATETIME UINT16_C(0x31) /* i64 ns since Unix epoch, UTC */
#define OMNI_TAG_CONSTRAINT UINT16_C(0x32) /* a constraint program, see 2.10 */
#define OMNI_TAG_CLIENT_RULE UINT16_C(0x33) /* matches a client, not a key */

/* Tag ranges. See the three-way tag decision in configstorage.md 12.1. */
#define OMNI_TAG_MIN_KNOWN UINT16_C(0x01)
#define OMNI_TAG_MAX_KNOWN UINT16_C(0x33)
#define OMNI_TAG_EXTENSION_BASE UINT16_C(0x8000)
#define OMNI_TAG_UNASSIGNED_0 UINT16_C(0x00)      /* never a live tag */
#define OMNI_TAG_UNASSIGNED_LO UINT16_C(0x0034)   /* hole in the core table */
#define OMNI_TAG_UNASSIGNED_HI UINT16_C(0x7FFF)

OMNI_STATIC_ASSERT(OMNI_TAG_MAX_KNOWN < OMNI_TAG_EXTENSION_BASE, "tag ranges");
OMNI_STATIC_ASSERT(OMNI_TAG_UNASSIGNED_LO == OMNI_TAG_MAX_KNOWN + 1, "tag gap");

/* region_ref: length is always 8, body_ref is the descriptor index, and
 * value_inline carries the descriptor generation. */
#define OMNI_REGION_REF_LENGTH UINT32_C(8)

/* --- constraint (0x32), a generic layout program --------------------- *
 *
 * A program is a packed array of fixed records with no header, so the record
 * count is derived from length and order is the evaluation order. It is the
 * named layout itself, stored once in SHM and shared by every client it
 * arranges, so it names no client: a record constrains the window the solver is
 * currently at, and that is decided by walk order rather than by the program.
 *
 * Consequently there is no epoch field and no entry identity anywhere in a
 * record. Walk order is over the scope the solve was given, which the engine
 * establishes from the tag list, and it is stable for the duration of a solve
 * because the set cannot change underneath the walk. */

#define OMNI_CONSTRAINT_RECORD_SIZE UINT32_C(16)
#define OMNI_CONSTRAINT_OFF_KIND UINT32_C(0)     /* u16 */
#define OMNI_CONSTRAINT_OFF_FLAGS UINT32_C(2)    /* u16 */
#define OMNI_CONSTRAINT_OFF_PRIORITY UINT32_C(4) /* u16, linear weight */
#define OMNI_CONSTRAINT_OFF_EDGES UINT32_C(6)    /* u8, edge mask */
#define OMNI_CONSTRAINT_OFF_AXIS UINT32_C(7)     /* u8 */
#define OMNI_CONSTRAINT_OFF_ROLE UINT32_C(8)     /* u32, walk index or ROLE_* */
#define OMNI_CONSTRAINT_OFF_LITERAL UINT32_C(12) /* i32 */

#define OMNI_CONSTRAINT_FILL_PARENT UINT16_C(0)
#define OMNI_CONSTRAINT_LEFT_OF UINT16_C(1)
#define OMNI_CONSTRAINT_HALF_WIDTH UINT16_C(2)
#define OMNI_CONSTRAINT_GAP_AROUND UINT16_C(3)
#define OMNI_CONSTRAINT_ADJACENT UINT16_C(4)
#define OMNI_CONSTRAINT_PLACE UINT16_C(5)
#define OMNI_CONSTRAINT_CONSTRAIN UINT16_C(6)
#define OMNI_CONSTRAINT_KIND_COUNT UINT16_C(7)

/* ROLE is the operand, expressed as a position in the walked set rather than as
 * a window. A role below ROLE_PARENT is an absolute walk index, so a program can
 * say "left-of the master" without knowing which window is the master, which is
 * the whole reason a generic layout can express master and stack. ROLE_PARENT is
 * the containing rect, which is not a walk member, and ROLE_NONE means the kind
 * takes no operand. The implied subject is always the current walk position, so
 * no flag is needed to say so. */
#define OMNI_CONSTRAINT_ROLE_PARENT UINT32_C(0xFFFFFFFE)
#define OMNI_CONSTRAINT_ROLE_NONE UINT32_C(0xFFFFFFFF)

#define OMNI_CONSTRAINT_FLAG_RATIO_0 (UINT16_C(1) << 0)
#define OMNI_CONSTRAINT_FLAG_RESERVED_MASK UINT16_C(0xFFFE)

/* Priority is the solver's weight, so the bands have to be separated by an
 * order of magnitude each for a band to dominate the compromise. Zero means no
 * opinion, which is what an absent record means, so the lowest useful value is
 * 1. DuckWM reaches the same conclusion with 1e8/1e4/1e2/1; see
 * research/duckwm.md.
 *
 * No band is a hard constraint. The solver minimises weighted violation, so two
 * constraints in the same band that cannot both hold still produce a compromise,
 * and DOMINANT is named for how much weight it carries rather than for a
 * guarantee it does not make. DuckWM's REQUIRED is the same shape: a large
 * weight the fit honours, not a separate code path. Nothing in the design needs
 * a genuine hard constraint, because generaldesign.md §7 makes every constraint
 * soft and prioritised, and a program that cannot be satisfied degrades rather
 * than fails. */
#define OMNI_CONSTRAINT_PRIORITY_DOMINANT UINT16_C(60000)
#define OMNI_CONSTRAINT_PRIORITY_STRONG UINT16_C(6000)
#define OMNI_CONSTRAINT_PRIORITY_MEDIUM UINT16_C(600)
#define OMNI_CONSTRAINT_PRIORITY_WEAK UINT16_C(60)
#define OMNI_CONSTRAINT_PRIORITY_BAND_COUNT UINT32_C(4)

#define OMNI_CONSTRAINT_AXIS_NONE UINT8_C(0)
#define OMNI_CONSTRAINT_AXIS_X UINT8_C(1)
#define OMNI_CONSTRAINT_AXIS_Y UINT8_C(2)
#define OMNI_CONSTRAINT_AXIS_XY UINT8_C(3)

/* Edge mask. The bit positions are the ones river's river_window_v1.set_tiled
 * and Hyprland's Layout::eRectCorner already use, so an adjacency computed here
 * is the same value those protocols carry. */
#define OMNI_EDGE_NONE UINT8_C(0)
#define OMNI_EDGE_TOP (UINT8_C(1) << 0)
#define OMNI_EDGE_BOTTOM (UINT8_C(1) << 1)
#define OMNI_EDGE_LEFT (UINT8_C(1) << 2)
#define OMNI_EDGE_RIGHT (UINT8_C(1) << 3)
#define OMNI_EDGE_ALL UINT8_C(0x0F)
#define OMNI_EDGE_MASK UINT8_C(0x0F)

OMNI_STATIC_ASSERT(OMNI_CONSTRAINT_OFF_LITERAL + 4 == OMNI_CONSTRAINT_RECORD_SIZE,
	"constraint record");
OMNI_STATIC_ASSERT(OMNI_CONSTRAINT_OFF_ROLE % 4 == 0, "role aligned");
OMNI_STATIC_ASSERT(OMNI_CONSTRAINT_OFF_LITERAL % 4 == 0, "literal aligned");
OMNI_STATIC_ASSERT(OMNI_CONSTRAINT_RECORD_SIZE % 4 == 0, "record 4-aligned");
OMNI_STATIC_ASSERT(OMNI_CONSTRAINT_ROLE_PARENT != OMNI_CONSTRAINT_ROLE_NONE,
	"role sentinels distinct");

/* --- client_rule (0x33) ---------------------------------------------- *
 *
 * OMNI_TAG_RULE matches a key and replaces a value. Every rule the layout set
 * needs matches a client and changes which scopes contain it, which is a
 * different record, so it gets its own tag rather than a version field. */

#define OMNI_CLIENT_RULE_OFF_MATCH_APPID UINT32_C(0)  /* u32 frame offset or NONE */
#define OMNI_CLIENT_RULE_OFF_MATCH_TITLE UINT32_C(4)  /* u32 frame offset or NONE */
#define OMNI_CLIENT_RULE_OFF_EFFECT UINT32_C(8)      /* u16 */
#define OMNI_CLIENT_RULE_OFF_FLAGS UINT32_C(10)      /* u16 */
#define OMNI_CLIENT_RULE_OFF_TARGET_ID UINT32_C(12)  /* u32 entry id */
#define OMNI_CLIENT_RULE_OFF_TARGET_GEN UINT32_C(16) /* u32 entry generation */
#define OMNI_CLIENT_RULE_OFF_SCOPE UINT32_C(20)      /* u16 */
#define OMNI_CLIENT_RULE_OFF_RESERVED UINT32_C(22)   /* u16, zero */
#define OMNI_CLIENT_RULE_HEADER_SIZE UINT32_C(24)
#define OMNI_CLIENT_RULE_OFF_PAYLOAD UINT32_C(24) /* the value the effect applies */

#define OMNI_CLIENT_RULE_EFFECT_NONE UINT16_C(0)
#define OMNI_CLIENT_RULE_EFFECT_JOIN_GROUP UINT16_C(1)
#define OMNI_CLIENT_RULE_EFFECT_LEAVE_GROUP UINT16_C(2)
#define OMNI_CLIENT_RULE_EFFECT_CLUSTER_ON_ATTACH UINT16_C(3)
#define OMNI_CLIENT_RULE_EFFECT_RELOCATE UINT16_C(4)
#define OMNI_CLIENT_RULE_EFFECT_SET_PROPERTY UINT16_C(5)
#define OMNI_CLIENT_RULE_EFFECT_SET_CONSTRAINTS UINT16_C(6)
#define OMNI_CLIENT_RULE_EFFECT_COUNT UINT16_C(7)

#define OMNI_CLIENT_RULE_SCOPE_MAP_ONLY UINT16_C(0) /* initial map only */
#define OMNI_CLIENT_RULE_SCOPE_ALWAYS UINT16_C(1)   /* re-resolved on reload */

#define OMNI_CLIENT_RULE_FLAG_TITLE_REGEX_0 (UINT16_C(1) << 0)
#define OMNI_CLIENT_RULE_FLAG_RESERVED_MASK UINT16_C(0xFFFE)

OMNI_STATIC_ASSERT(OMNI_CLIENT_RULE_OFF_RESERVED + 2 == OMNI_CLIENT_RULE_HEADER_SIZE,
	"client rule header");
OMNI_STATIC_ASSERT(OMNI_CLIENT_RULE_OFF_TARGET_ID % 4 == 0, "target aligned");

/* --- binding (0x2B), now framed -------------------------------------- *
 *
 * A binding carries its action's positional arguments, so a snap can take a
 * direction and wm.cycle_layout can take a layout name. The argument array is
 * inline after a fixed header and holds string values, which is the shape
 * ipc.md's exec already uses. args_ref names the array's arena frame so a
 * reader can find it without walking the payload. */

#define OMNI_BINDING_OFF_MODMASK UINT32_C(0)     /* u32 */
#define OMNI_BINDING_OFF_KEYSYM UINT32_C(4)      /* u32 */
#define OMNI_BINDING_OFF_KEYCODE UINT32_C(8)     /* u32 */
#define OMNI_BINDING_OFF_ACTION_REF UINT32_C(12) /* u32 frame offset */
#define OMNI_BINDING_OFF_ARGS_REF UINT32_C(16)   /* u32 frame offset or OMNI_REF_NONE */
#define OMNI_BINDING_OFF_FLAGS UINT32_C(20)      /* u32 */
#define OMNI_BINDING_HEADER_SIZE UINT32_C(24)
#define OMNI_BINDING_OFF_ARGS UINT32_C(24) /* array value, inline */

#define OMNI_BINDING_FLAG_USE_KEYSYM_0 (UINT32_C(1) << 0)
#define OMNI_BINDING_FLAG_RESERVED_MASK UINT32_C(0xFFFFFFFE)

OMNI_STATIC_ASSERT(OMNI_BINDING_OFF_FLAGS + 4 == OMNI_BINDING_HEADER_SIZE,
	"binding header");
OMNI_STATIC_ASSERT(OMNI_BINDING_HEADER_SIZE % 4 == 0, "binding header aligned");

/* --- solved layout section ------------------------------------------- *
 *
 * One record per placed node. The identity is entry id plus entry generation,
 * with the epoch implied, because a solve only concerns the current epoch.
 * The four extents are the solve output and nothing else: a node is placed,
 * and what it is placed against is in the program, not here. */

#define OMNI_SOLVED_OFF_GENERATION UINT32_C(0) /* u64, bumped once per solve */
#define OMNI_SOLVED_OFF_NODE_COUNT UINT32_C(8) /* u32 */
#define OMNI_SOLVED_OFF_RESERVED UINT32_C(12)  /* u32, zero */
#define OMNI_SOLVED_OFF_NODES UINT32_C(16)
#define OMNI_SOLVED_NODE_OFF_REF_ID UINT32_C(0)
#define OMNI_SOLVED_NODE_OFF_REF_GEN UINT32_C(4)
#define OMNI_SOLVED_NODE_OFF_X UINT32_C(8)  /* i32, canvas coordinates */
#define OMNI_SOLVED_NODE_OFF_Y UINT32_C(12) /* i32 */
#define OMNI_SOLVED_NODE_OFF_W UINT32_C(16) /* i32 */
#define OMNI_SOLVED_NODE_OFF_H UINT32_C(20) /* i32 */

OMNI_STATIC_ASSERT(OMNI_SOLVED_OFF_RESERVED + 4 == OMNI_SOLVED_OFF_NODES,
	"solved header");
OMNI_STATIC_ASSERT(OMNI_SOLVED_NODE_OFF_H + 4 == OMNI_SOLVED_NODE_SIZE,
	"solved node record");
OMNI_STATIC_ASSERT(OMNI_SOLVED_OFF_NODES % 4 == 0, "solved nodes aligned");

/* --- layout slots ------------------------------------------------------ *
 *
 * The count and the list are edited together; a closed set is only checkable
 * if it is enumerable, so the names are literals and are never generated from
 * a rule. Twenty-four is the whole Greek alphabet, which is the reason the set
 * is closed: extending past it would mean inventing names, and a slot is only
 * useful while it is an address with no meaning of its own. */

/* One namespace, two provenances. The letters name user layouts and the
 * descriptive names name built-ins, and nothing else separates them: both are
 * named programs of spaces and constraints resolved by the same lookup. A Greek
 * letter cannot collide with a descriptive name, so a user layout can never
 * shadow a built-in without any precedence rule or reserved list. Every letter
 * is seeded as a blank program at startup, which is valid, not absent. */
#define OMNI_LAYOUT_SLOT_COUNT UINT32_C(24)
#define OMNI_LAYOUT_SLOT_NAMES                                    \
	"alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta", \
	    "theta", "iota", "kappa", "lambda", "mu", "nu", "xi",    \
	    "omicron", "pi", "rho", "sigma", "tau", "upsilon", "phi", \
	    "chi", "psi", "omega"

/* Bounds on a group naming a nested layout. Both are checked when a program is
 * loaded, never while solving, so a rejected program leaves the running layout
 * untouched. Acyclicity is the one that matters: alpha with two spaces that are
 * both groups using alpha cascades forever and hangs the compositor rather than
 * producing a visible mistake. It also caps depth at the number of layouts (26,
 * being the 24 letters chained with a built-in at the end) for free. The cap of
 * 5 sits well inside that, at the point past which nesting is not readable on
 * screen, and it is what makes the arrange pass's cost bounded by a constant
 * factor rather than merely safe. */
#define OMNI_LAYOUT_MAX_NEST_DEPTH UINT32_C(5)

/* ------------------------------------------------------------------ */
/* 8. Journal ring (configstorelayout.md section 8)                   */
/* ------------------------------------------------------------------ */

#define OMNI_JOURNAL_META_OFF_HEAD UINT32_C(0)             /* u32 */
#define OMNI_JOURNAL_META_OFF_COUNT UINT32_C(4)            /* u32 */
#define OMNI_JOURNAL_META_OFF_OLDEST_SEQ UINT32_C(8)       /* u64 */
#define OMNI_JOURNAL_META_OFF_NEXT_SEQ UINT32_C(16)        /* u64 */
#define OMNI_JOURNAL_META_OFF_PUBLISH_SEQ UINT32_C(24)     /* u64 seqlock */

#define OMNI_JOURNAL_SLOT_OFF_EPOCH UINT32_C(0)
#define OMNI_JOURNAL_SLOT_OFF_COMMIT_ID UINT32_C(8)
#define OMNI_JOURNAL_SLOT_OFF_JOURNAL_SEQ UINT32_C(16)
#define OMNI_JOURNAL_SLOT_OFF_TIME_NS UINT32_C(24)
#define OMNI_JOURNAL_SLOT_OFF_ENTRY_GENERATION UINT32_C(32)
#define OMNI_JOURNAL_SLOT_OFF_ENTRY_ID UINT32_C(40) /* u32 */
#define OMNI_JOURNAL_SLOT_OFF_TYPE_TAG UINT32_C(44) /* u16 */
#define OMNI_JOURNAL_SLOT_OFF_KIND UINT32_C(46)     /* u8  */
#define OMNI_JOURNAL_SLOT_OFF_FLAGS UINT32_C(47)    /* u8  */
#define OMNI_JOURNAL_SLOT_OFF_VALUE_INLINE UINT32_C(48) /* u64 */
#define OMNI_JOURNAL_SLOT_OFF_BODY_REF UINT32_C(56)    /* u32 */
#define OMNI_JOURNAL_SLOT_OFF_EVENT_REF UINT32_C(60)   /* u32 */

/* Journal entry kinds. */
#define OMNI_JOURNAL_KIND_KEY_SET UINT8_C(1)
#define OMNI_JOURNAL_KIND_KEY_DELETE UINT8_C(2)
#define OMNI_JOURNAL_KIND_EVENT UINT8_C(3)
#define OMNI_JOURNAL_KIND_COMMIT_END UINT8_C(4) /* event_ref counts the group */

/* Journal entry flags. */
#define OMNI_JOURNAL_FLAG_VALUE_FRAMED_0 (UINT8_C(1) << 0)
#define OMNI_JOURNAL_FLAG_ENTRY_VALID_1 (UINT8_C(1) << 1)
#define OMNI_JOURNAL_FLAG_EVENT_REF_VALID_2 (UINT8_C(1) << 2)
#define OMNI_JOURNAL_FLAG_RESERVED_MASK UINT8_C(0xFC)

OMNI_STATIC_ASSERT(OMNI_JOURNAL_SLOT_OFF_VALUE_INLINE % 8 == 0, "slot u64 aligned");
OMNI_STATIC_ASSERT(OMNI_JOURNAL_SLOT_OFF_COMMIT_ID % 8 == 0, "slot commit aligned");

/* Slot i begins at OMNI_JOURNAL_OFF + OMNI_JOURNAL_HEADER_SIZE + i * size. */
#define OMNI_JOURNAL_SLOT_ADDR(base, index) \
	((base) + OMNI_JOURNAL_HEADER_SIZE + (index) * OMNI_JOURNAL_SLOT_SIZE)

/* A gap exists when the last-seen cursor is older than the retained window.
 * The oldest_seq == 0 guard is not cosmetic: the ring is empty before the
 * first append, and an unguarded unsigned subtract would wrap to a huge value
 * and report a gap that does not exist. */
#define OMNI_JOURNAL_HAS_GAP(cursor_seq, oldest_seq)                       \
	(((oldest_seq) == UINT64_C(0)) ||                                     \
		((cursor_seq) < ((oldest_seq) - UINT64_C(1))))

/* ------------------------------------------------------------------ */
/* 9. Request queue (configstorelayout.md section 9)                   */
/* ------------------------------------------------------------------ */

#define OMNI_REQ_OFF_STATUS UINT32_C(0)          /* u8 */
#define OMNI_REQ_OFF_TYPE UINT32_C(1)            /* u8 */
#define OMNI_REQ_OFF_RESULT_CODE UINT32_C(2)     /* u8 */
#define OMNI_REQ_OFF_FLAGS UINT32_C(3)           /* u8, reserved */
#define OMNI_REQ_OFF_TICKET UINT32_C(4)          /* u32 */
#define OMNI_REQ_OFF_EPOCH UINT32_C(8)           /* u64 */
#define OMNI_REQ_OFF_REQUESTER_PID UINT32_C(16)  /* u32 */
#define OMNI_REQ_OFF_TARGET UINT32_C(20)         /* u32 */
#define OMNI_REQ_OFF_TYPE_TAG UINT32_C(24)       /* u16 */
#define OMNI_REQ_OFF_REG_FORMAT UINT32_C(26)     /* u16 */
#define OMNI_REQ_OFF_WIDTH UINT32_C(28)          /* u32 */
#define OMNI_REQ_OFF_HEIGHT UINT32_C(32)         /* u32 */
#define OMNI_REQ_OFF_SLOT_COUNT UINT32_C(36)     /* u8 */
#define OMNI_REQ_OFF_VALUE_LEN UINT32_C(40)      /* u32 */
#define OMNI_REQ_OFF_NAME_LEN UINT32_C(44)       /* u32 */
#define OMNI_REQ_OFF_TARGET_GENERATION UINT32_C(48) /* u64 */
#define OMNI_REQ_OFF_NAME UINT32_C(56)           /* 4096 B */
#define OMNI_REQ_NAME_FIELD_SIZE UINT32_C(4096)
#define OMNI_REQ_OFF_VALUE UINT32_C(4152) /* 128 B */
#define OMNI_REQ_VALUE_FIELD_SIZE UINT32_C(128)
#define OMNI_REQ_OFF_REQUESTER_START_ID UINT32_C(4280) /* u32 */
#define OMNI_REQ_OFF_RESERVED_PAD UINT32_C(4284) /* u32, zero; 8-aligns the next field */
#define OMNI_REQ_OFF_TERMINAL_AT_MS UINT32_C(4288)     /* u64 */
#define OMNI_REQ_RESERVED_TAIL_START UINT32_C(4296)

/* Request status. A request is FREE only via acknowledgement or reclaim. */
#define OMNI_REQ_STATUS_FREE UINT8_C(0)
#define OMNI_REQ_STATUS_PENDING UINT8_C(1)
#define OMNI_REQ_STATUS_DONE UINT8_C(2)
#define OMNI_REQ_STATUS_ERROR UINT8_C(3)

/* Request types. */
#define OMNI_REQ_TYPE_CREATE_ENTRY UINT8_C(1)
#define OMNI_REQ_TYPE_CREATE_REGION UINT8_C(2)
#define OMNI_REQ_TYPE_DESTROY_ENTRY UINT8_C(3)
#define OMNI_REQ_TYPE_DESTROY_REGION UINT8_C(4)

/* Request result codes, stored in the slot and echoed by the socket. */
#define OMNI_REQ_OK UINT8_C(0)
#define OMNI_REQ_ERR_PARAM_INVALID UINT8_C(1)
#define OMNI_REQ_ERR_NAME_TOO_LONG UINT8_C(2)
#define OMNI_REQ_ERR_CATALOG_FULL UINT8_C(3)
#define OMNI_REQ_ERR_ARENA_FULL UINT8_C(4)
#define OMNI_REQ_ERR_REGION_FULL UINT8_C(5)
#define OMNI_REQ_ERR_REGION_TOO_LARGE UINT8_C(6)
#define OMNI_REQ_ERR_BLOCK_EXHAUSTED UINT8_C(7)
#define OMNI_REQ_ERR_BAD_TARGET UINT8_C(8)
#define OMNI_REQ_ERR_STORE_BROKEN UINT8_C(9)
#define OMNI_REQ_ERR_NOT_READY UINT8_C(10)
#define OMNI_REQ_ERR_REQUEST_EXPIRED UINT8_C(11)

OMNI_STATIC_ASSERT(OMNI_REQ_OFF_NAME + OMNI_REQ_NAME_FIELD_SIZE == OMNI_REQ_OFF_VALUE,
	"name field");
OMNI_STATIC_ASSERT(OMNI_REQ_OFF_VALUE + OMNI_REQ_VALUE_FIELD_SIZE ==
                   OMNI_REQ_OFF_REQUESTER_START_ID,
	"value field");
OMNI_STATIC_ASSERT(OMNI_REQ_OFF_REQUESTER_START_ID + 4 == OMNI_REQ_OFF_RESERVED_PAD,
	"start id field");
OMNI_STATIC_ASSERT(OMNI_REQ_OFF_RESERVED_PAD + 4 == OMNI_REQ_OFF_TERMINAL_AT_MS,
	"pad field");
OMNI_STATIC_ASSERT(OMNI_REQ_OFF_TERMINAL_AT_MS + 8 == OMNI_REQ_RESERVED_TAIL_START,
	"terminal deadline field");
OMNI_STATIC_ASSERT(OMNI_REQ_RESERVED_TAIL_START <= OMNI_REQUEST_SLOT_SIZE,
	"reserved tail fits the slot");
OMNI_STATIC_ASSERT(OMNI_REQ_OFF_EPOCH % 8 == 0, "req epoch aligned");
OMNI_STATIC_ASSERT(OMNI_REQ_OFF_TARGET_GENERATION % 8 == 0, "req generation aligned");
OMNI_STATIC_ASSERT(OMNI_REQ_OFF_TERMINAL_AT_MS % 8 == 0, "req deadline aligned");

/* ------------------------------------------------------------------ */
/* 10. Region descriptor (configstorelayout.md section 10)             */
/* ------------------------------------------------------------------ */

#define OMNI_RDESC_OFF_PAYLOAD_OFFSET UINT32_C(0) /* u32 */
#define OMNI_RDESC_OFF_PAYLOAD_BYTES UINT32_C(4)  /* u32 */
#define OMNI_RDESC_OFF_STRIDE UINT32_C(8)         /* u32 */
#define OMNI_RDESC_OFF_WIDTH UINT32_C(12)         /* u16 */
#define OMNI_RDESC_OFF_HEIGHT UINT32_C(14)        /* u16 */
#define OMNI_RDESC_OFF_FORMAT UINT32_C(16)        /* u8  */
#define OMNI_RDESC_OFF_SLOT_COUNT UINT32_C(17)    /* u8  */
#define OMNI_RDESC_OFF_FLAGS UINT32_C(18)         /* u8  */
#define OMNI_RDESC_OFF_RESERVED UINT32_C(19)      /* u8  */
#define OMNI_RDESC_OFF_REGION_REVISION UINT32_C(20) /* u32 */
#define OMNI_RDESC_OFF_REGION_GENERATION UINT32_C(24) /* u64 */

/* Descriptor flags. Protocol markers, not ownership or access control. */
#define OMNI_RDESC_FLAG_IN_USE_0 (UINT8_C(1) << 0)
#define OMNI_RDESC_FLAG_DESTROYED_1 (UINT8_C(1) << 1)
#define OMNI_RDESC_FLAG_PRODUCER_ACTIVE_2 (UINT8_C(1) << 2)
#define OMNI_RDESC_FLAG_CONSUMER_ACTIVE_3 (UINT8_C(1) << 3)
#define OMNI_RDESC_FLAG_RESERVED_MASK UINT8_C(0xF0)

/* Pixel formats. All are 4 bytes per pixel. */
#define OMNI_PIXEL_FORMAT_ARGB8888 UINT8_C(0)
#define OMNI_PIXEL_FORMAT_RGBA8888 UINT8_C(1)
#define OMNI_PIXEL_FORMAT_XRGB8888 UINT8_C(2)
#define OMNI_PIXEL_FORMAT_COUNT UINT8_C(3)
#define OMNI_PIXEL_BYTES UINT32_C(4)

/* Revision 0 means unpublished; the first published revision is 1. */
#define OMNI_REGION_REVISION_UNPUBLISHED UINT32_C(0)
#define OMNI_REGION_GENERATION_MIN UINT64_C(1)

/* Double-buffered slot selection for a published revision r. */
#define OMNI_REGION_SLOT_FOR_REV(revision) ((revision) & UINT32_C(1))

/* Frame bytes for one image, overflow-checked by the caller. */
#define OMNI_REGION_FRAME_BYTES(stride, height) ((stride) * (height))

/* Trailing payload allocation, rounded up to OMNI_ALIGN. */
#define OMNI_REGION_ALLOC_SIZE(frame_bytes, slots) \
	OMNI_ALIGN_UP((frame_bytes) * (slots), OMNI_ALIGN)

OMNI_STATIC_ASSERT(OMNI_RDESC_OFF_REGION_GENERATION + 8 == OMNI_REGION_DESC_SIZE,
	"descriptor tail");
OMNI_STATIC_ASSERT(OMNI_RDESC_OFF_REGION_GENERATION % 8 == 0, "generation aligned");

#endif /* OMNIWM_SHARED_OMNI_LAYOUT_H */

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
 *   1. OMNI_TAG_CONSTRAINT (0x32), a layout program as a nestable value, so a
 *      layout holds a tree of named spaces rather than a positional tuple. It
 *      began as a packed array of 16-byte records, which cannot hold a nested
 *      program at all; devnotes/layoutlanguage.md section 3 makes nesting the
 *      only way to use more than one arrangement, so the payload became the
 *      tree and the tag stayed to mark the domain.
 *   2. OMNI_TAG_CLIENT_RULE (0x33), because OMNI_TAG_RULE matches keys and
 *      replaces values, and the client rules solve soft equations against a
 *      reference instead. It is one 24-byte record per rule, with no payload,
 *      and it carries no matchers: the matcher selects which sets a client is
 *      bound to, and it lives in the window-rule block, not in this record.
 *   3. OMNI_TAG_BINDING is now framed and carries an argument list, because a
 *      snap-by-keybind takes a direction and wm.cycle_layout already takes a
 *      layout name in ipc.md's own example.
 *   4. OMNI_SECTION_SOLVED_LAYOUT, a non-journalled section holding the most
 *      recent solve, so an external program can read a layout and a solve does
 *      not cost a commit.
 *
 * One tag has since been added that is not from that list, because
 * devnotes/windows.md section 9.3 needed it rather than the layout engine:
 * OMNI_TAG_MAP (0x34), a keyed block of typed values. The store had no
 * composite value with named fields, since devnotes/configstorage.md section 3
 * is an open key-value catalog where a TOML table flattens into a key path
 * instead of becoming a value, and a window rule is the first thing that needs a
 * block to be a value. A window rule is a map with two well-known keys, "if"
 * and "then"; it gets no tag of its own, for the reason devnotes/
 * configstorage.md section 4 gives when it dropped the effect enum from
 * client_rule: the namespace at omniwm.window_rules.<name> already says what
 * the value is, and a tag repeating it is a second place for the two to
 * disagree.
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

/* Catalog name index. A second, sorted view of the catalog: one slot per
 * catalog slot, sorted by (name_hash, entry_id), binary searched. This exists
 * so `get` never walks the catalog. It is maintained transactionally by the
 * writer under the commit futex, so it is never stale and never rebuilt.
 * The catalog itself is NOT sorted, because entry_id is the slot index and
 * live entries never move; sorting them in place would invalidate every held
 * reference and make delete O(n) on the catalog. */
#define OMNI_CATALOG_INDEX_OFF UINT32_C(0x81000)
#define OMNI_CATALOG_INDEX_HEADER_SIZE UINT32_C(32)
#define OMNI_CATALOG_INDEX_SLOT_SIZE UINT32_C(16)
#define OMNI_CATALOG_INDEX_SLOT_COUNT UINT32_C(16384) /* == catalog slots */
#define OMNI_CATALOG_INDEX_ENTRY_ID_TOMB UINT32_C(0xFFFFFFFF)

/* Name hash. FNV-1a 64 over the name bytes, then a splitmix64 finalizer.
 * The finalizer is not optional: FNV-1a's high bits mix poorly for short
 * inputs, and this index is *sorted* by the full 64-bit value, so the binary
 * search discriminates on the high bits first and needs them to be good. */
#define OMNI_NAME_HASH_OFFSET_BASIS UINT64_C(0xCBF29CE484222325)
#define OMNI_NAME_HASH_PRIME UINT64_C(0x100000001B3)
#define OMNI_NAME_HASH_SPLITMIX_GAMMA UINT64_C(0x9E3779B97F4A7C15)

#define OMNI_CATALOG_INDEX_OFF_HASH UINT32_C(0)      /* u64, sorted ascending */
#define OMNI_CATALOG_INDEX_OFF_ENTRY_ID UINT32_C(8) /* u32 */
#define OMNI_CATALOG_INDEX_OFF_GENERATION_LO UINT32_C(12) /* u32, low half */

#define OMNI_CATALOG_INDEX_HDR_OFF_LIVE UINT32_C(0)     /* u32 */
#define OMNI_CATALOG_INDEX_HDR_OFF_USED UINT32_C(4)     /* u32, live + tombstones */
#define OMNI_CATALOG_INDEX_HDR_OFF_VERSION UINT32_C(8)  /* u32, see below */
#define OMNI_CATALOG_INDEX_HDR_OFF_RESERVED_0 UINT32_C(12) /* u32, zero */

/* Layout version of the index itself, stored at +8. This is a second version
 * from OMNI_FORMAT_VERSION on purpose: the index is the one section whose shape
 * a reader cannot infer. The other sections are located by constants in this
 * header, so a reader that has a different constant for one already refuses the
 * block. The index has no such anchor: an index written by a build that added,
 * dropped, or re-ordered a slot field still has a valid 32-byte header and a
 * valid sorted array, so a reader would binary-search it happily and get wrong
 * answers rather than an error. The version is what turns that into a refusal.
 * It is bumped only for a change to the header or slot layout above, never for a
 * change to the algorithm that maintains them; a reader that sees a mismatch
 * returns OMNI_ERR_BLOCK_UNSUPPORTED and does not fall back to a catalog scan,
 * because a scan is exactly the cost the index exists to avoid. */
#define OMNI_CATALOG_INDEX_VERSION UINT32_C(1)

#define OMNI_JOURNAL_OFF UINT32_C(0xC2000)
#define OMNI_JOURNAL_HEADER_SIZE UINT32_C(32)
#define OMNI_JOURNAL_CAPACITY UINT32_C(4096)
#define OMNI_JOURNAL_SLOT_SIZE UINT32_C(64)
#define OMNI_JOURNAL_INLINE_LIMIT UINT32_C(8) /* values <= 8 B go inline */

#define OMNI_REQUESTS_OFF UINT32_C(0x103000)
#define OMNI_REQUEST_SLOT_COUNT UINT32_C(256)
/* The name lives in the arena, referenced by body_ref, so a slot is 256 bytes
 * instead of 0x1100. The queue was 1.1MB, about 58% of the block's fixed
 * area, almost all of it 4096-byte inline name buffers. */
#define OMNI_REQUEST_SLOT_SIZE UINT32_C(0x100)
#define OMNI_REQUEST_NAME_MAX UINT32_C(4095)  /* bytes including the NUL */
#define OMNI_REQUEST_VALUE_MAX UINT32_C(128)  /* inline initial-value cap */
/* Terminal-slot reclaim deadline. One definition; only the WM reads it. */
#define OMNI_REQUEST_RECLAIM_MS UINT32_C(5000)

#define OMNI_REGION_DESC_OFF UINT32_C(0x113000)
#define OMNI_REGION_DESC_SIZE UINT32_C(32)
#define OMNI_REGION_DESC_COUNT UINT32_C(64)
#define OMNI_REGION_SLOT_COUNT_MAX UINT32_C(2) /* double buffering is 1 or 2 */
#define OMNI_REGION_FRAME_MAX_BYTES (UINT32_C(34) * 1024 * 1024)
#define OMNI_REGION_TOTAL_MAX_BYTES (UINT32_C(68) * 1024 * 1024)

/* Solved layout section. Fixed, single-buffered, never journalled and never
 * saved: it holds the geometry the most recent solve produced, so a script can
 * read a layout, and it is overwritten rather than appended on every pass. */
#define OMNI_SOLVED_OFF UINT32_C(0x113800)
#define OMNI_SOLVED_SECTION_SIZE UINT32_C(0x8000)
#define OMNI_SOLVED_HEADER_SIZE UINT32_C(16)
#define OMNI_SOLVED_NODE_SIZE UINT32_C(24)
#define OMNI_SOLVED_SLOT_COUNT_MAX UINT32_C(1024)
/* Bytes the header plus a full node array occupies. Must fit the section. */
#define OMNI_SOLVED_BYTES \
	(OMNI_SOLVED_HEADER_SIZE + OMNI_SOLVED_NODE_SIZE * OMNI_SOLVED_SLOT_COUNT_MAX)

#define OMNI_POOL_OFF UINT32_C(0x11C000) /* == FIXED_END, see section 3 */

/* Sentinel stored in place of a pool offset or slot index meaning "none". */
#define OMNI_REF_NONE UINT32_C(0xFFFFFFFF)

/* --- Arena frame header (every framed payload) -------------------- *
 *
 * A frame is 16 bytes of header plus a payload, allocated 16-aligned. The
 * header is shared by live frames and free frames, so recycling costs no
 * extra space: a free frame stores its chain link and its total size in the
 * bytes that are reserved for a live frame.
 *
 *   live:  +0 length (payload bytes)  +4 next_free = 0
 *          +8..15 reserved, zero
 *   free:  +0 total frame bytes       +4 next_free (frame offset or NONE)
 *          +8 free_size (== +0)      +12 reserved
 *
 * A reader distinguishes them by next_free: a live frame has zero, a free
 * frame has a real offset or OMNI_REF_NONE for the tail of the list. */
#define OMNI_FRAME_OFF_LENGTH UINT32_C(0)     /* u32 */
#define OMNI_FRAME_OFF_NEXT_FREE UINT32_C(4)  /* u32, 0 when live */
#define OMNI_FRAME_OFF_FREE_SIZE UINT32_C(8)  /* u32, == length when free */
#define OMNI_FRAME_HEADER_SIZE UINT32_C(16)
#define OMNI_FRAME_ALIGN UINT32_C(16)

/* Bytes a framed payload of `payload_len` occupies, header included. */
#define OMNI_FRAME_BYTES(payload_len) \
	OMNI_ALIGN_UP(OMNI_FRAME_HEADER_SIZE + (payload_len), OMNI_FRAME_ALIGN)

/* ------------------------------------------------------------------ */
/* 3. Derived geometry, asserted at build time                         */
/* ------------------------------------------------------------------ */

#define OMNI_CATALOG_SIZE (OMNI_CATALOG_SLOT_COUNT * OMNI_CATALOG_ENTRY_SIZE)
#define OMNI_CATALOG_INDEX_SIZE \
	(OMNI_CATALOG_INDEX_HEADER_SIZE + \
	 OMNI_CATALOG_INDEX_SLOT_COUNT * OMNI_CATALOG_INDEX_SLOT_SIZE)
#define OMNI_JOURNAL_SIZE \
	(OMNI_JOURNAL_HEADER_SIZE + OMNI_JOURNAL_CAPACITY * OMNI_JOURNAL_SLOT_SIZE)
#define OMNI_REQUESTS_SIZE (OMNI_REQUEST_SLOT_COUNT * OMNI_REQUEST_SLOT_SIZE)
#define OMNI_REGION_DESC_TOTAL (OMNI_REGION_DESC_COUNT * OMNI_REGION_DESC_SIZE)
#define OMNI_FIXED_END OMNI_POOL_OFF
#define OMNI_INITIAL_POOL (OMNI_BLOCK_INITIAL_SIZE - OMNI_FIXED_END)
#define OMNI_MAX_POOL (OMNI_BLOCK_MAX_SIZE - OMNI_FIXED_END)

OMNI_STATIC_ASSERT(OMNI_CATALOG_SIZE == UINT32_C(0x80000), "catalog size");
OMNI_STATIC_ASSERT(OMNI_CATALOG_INDEX_SIZE == UINT32_C(0x40020),
	"catalog index size");
OMNI_STATIC_ASSERT(OMNI_CATALOG_INDEX_SLOT_COUNT == OMNI_CATALOG_SLOT_COUNT,
	"catalog index covers catalog");
OMNI_STATIC_ASSERT(OMNI_JOURNAL_SIZE == UINT32_C(0x40020), "journal size");
OMNI_STATIC_ASSERT(OMNI_REQUESTS_SIZE == UINT32_C(0x10000), "request size");
OMNI_STATIC_ASSERT(OMNI_REGION_DESC_TOTAL == UINT32_C(0x800), "descriptor size");
OMNI_STATIC_ASSERT(OMNI_SOLVED_BYTES == UINT32_C(0x6010), "solved layout bytes");
OMNI_STATIC_ASSERT(OMNI_SOLVED_BYTES <= OMNI_SOLVED_SECTION_SIZE,
	"solved layout fits its section");
OMNI_STATIC_ASSERT(OMNI_SOLVED_OFF + OMNI_SOLVED_SECTION_SIZE <= OMNI_POOL_OFF,
	"solved layout section");

/* Section bases are derived, so assert the derivation rather than the number. */
OMNI_STATIC_ASSERT(OMNI_CATALOG_INDEX_OFF ==
                   OMNI_ALIGN_UP(OMNI_CATALOG_OFF + OMNI_CATALOG_SIZE, 0x1000),
	"catalog index offset");
OMNI_STATIC_ASSERT(OMNI_JOURNAL_OFF ==
                   OMNI_ALIGN_UP(OMNI_CATALOG_INDEX_OFF + OMNI_CATALOG_INDEX_SIZE,
                                 0x1000),
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
OMNI_STATIC_ASSERT(OMNI_CATALOG_INDEX_OFF % 0x1000 == 0,
	"catalog index page aligned");
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
#define OMNI_HDR_OFF_ARENA_FREE_HEAD UINT32_C(164) /* u32 frame offset or OMNI_REF_NONE */
#define OMNI_HDR_RESERVED_TAIL_START UINT32_C(168) /* through offset 255 */

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
#define OMNI_SECTION_CATALOG_INDEX UINT32_C(8)

/* Section table row field offsets. */
#define OMNI_SECTION_ROW_OFF_ID UINT32_C(0)
#define OMNI_SECTION_ROW_OFF_OFFSET UINT32_C(4)
#define OMNI_SECTION_ROW_OFF_SIZE UINT32_C(8)
#define OMNI_SECTION_ROW_OFF_FLAGS UINT32_C(16)
#define OMNI_SECTION_ROW_OFF_RESERVED UINT32_C(20)

/* Section row flags. Protocol markers, not ownership or access control.
 * A section without JOURNALLED is written by the producer alone and is
 * neither appended to the journal nor included by a save. That is the
 * solved layout section's whole reason for existing.
 *
 * A reader that cannot validate the CATALOG_INDEX row refuses `get`. There is
 * no catalog scan behind it, because a walk's cost scales with how many keys
 * the user has configured, and a cost that depends on unrelated configuration
 * is one nobody profiles. The refusal's result code is not defined yet and is
 * an open item in `configstorelayout.md` §14. */
#define OMNI_SECTION_FLAG_PRESENT_0 (UINT32_C(1) << 0)
#define OMNI_SECTION_FLAG_JOURNALLED_1 (UINT32_C(1) << 1)
#define OMNI_SECTION_FLAG_RESERVED_MASK UINT32_C(0xFFFFFFFC)

/* Fields accessed with aligned atomics, and their required alignment. */
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_FUTEX % 4 == 0, "futex aligned");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_COMMIT_STATE % 4 == 0, "commit_state aligned");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_ACTIVE_COMMIT_ID % 8 == 0, "active id aligned");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_READY % 4 == 0, "ready aligned");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_READY + 4 <= OMNI_HDR_OFF_ARENA_FREE_HEAD,
	"ready precedes the free-list head");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_ARENA_FREE_HEAD % 4 == 0, "free head aligned");
OMNI_STATIC_ASSERT(OMNI_HDR_OFF_ARENA_FREE_HEAD + 4 ==
                   OMNI_HDR_RESERVED_TAIL_START,
	"free head is the last defined field");
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
#define OMNI_TAG_MAP UINT16_C(0x34)        /* a keyed block of typed values */

/* Tag ranges. See the three-way tag decision in configstorage.md 12.1. */
#define OMNI_TAG_MIN_KNOWN UINT16_C(0x01)
#define OMNI_TAG_MAX_KNOWN UINT16_C(0x34)
#define OMNI_TAG_EXTENSION_BASE UINT16_C(0x8000)
#define OMNI_TAG_UNASSIGNED_0 UINT16_C(0x00)      /* never a live tag */
#define OMNI_TAG_UNASSIGNED_LO UINT16_C(0x0035)   /* hole in the core table */
#define OMNI_TAG_UNASSIGNED_HI UINT16_C(0x7FFF)

OMNI_STATIC_ASSERT(OMNI_TAG_MAX_KNOWN < OMNI_TAG_EXTENSION_BASE, "tag ranges");
OMNI_STATIC_ASSERT(OMNI_TAG_UNASSIGNED_LO == OMNI_TAG_MAX_KNOWN + 1, "tag gap");

/* region_ref: length is always 8, body_ref is the descriptor index, and
 * value_inline carries the descriptor generation. */
#define OMNI_REGION_REF_LENGTH UINT32_C(8)

/* --- constraint (0x32), a layout program ----------------------------- *
 *
 * A program is a tree, not a record. devnotes/layoutlanguage.md section 3
 * makes a program a list of spaces where a space may name a nested layout, and
 * nesting is the only way to use more than one arrangement, so the payload is:
 *
 *   constraint := array of space
 *   space      := option, framed: the space's name, then its value
 *   value      := the space's own arguments, which are OMNI_TAG_ENUM,
 *                  OMNI_TAG_STRING and integer values, or
 *                | constraint, wherever a space names a nested layout
 *
 * Every leaf is a tag that already existed, so this block holds no offsets and
 * no sizes: there is nothing here to drift. The one number the program form
 * has is the nesting bound, OMNI_LAYOUT_MAX_NEST_DEPTH below.
 *
 * This replaced a packed array of 16-byte records, and the reason is worth
 * keeping because the tag was never the problem. That record had a kind, an
 * edge mask, an axis, a literal, a role and a priority in fixed fields, which
 * is one rule per record with every argument present whether or not that rule
 * uses it, and a flat array has nowhere to put a child program. The language
 * instead states each rule with only the arguments its own kind takes, and a
 * program that names a nested layout is a program within a program. The tag
 * stayed at 0x32 so a core reader can recognise a layout program without a
 * schema, which is the one thing the record was buying.
 *
 * The record also had a priority, and its absence below is not an oversight.
 * There is no priority in the language: rules are tried in order and the first
 * that fits wins, so order in the array is the whole of it and a weight field
 * would have been a second, conflicting statement of the same thing. */

/* Two weights, one per solved family, and that is the whole of it. The four
 * named decades this replaces (DOMINANT 60000, STRONG 6000, MEDIUM 600, WEAK
 * 60) were a leftover from the constraint-record era, and the check that
 * retired them is worth recording: nothing designated them. They were assigned
 * by mapping "positional constraints" to STRONG and "structural constraints" to
 * DOMINANT, but the current language has no positional or structural
 * constraint, because a program rule is decided by its position in the list and
 * never reaches the solver at all. Three of the four had no caller even then.
 * A band with no caller is not a weight, it is a number that looks like a
 * policy.
 *
 * A weight is still needed, and it is needed for exactly one thing: breaking a
 * tie inside the solve. Two relations that cannot both hold need a defined
 * winner, or the solver's output depends on visit order and §3.2's determinism
 * requirement is unsatisfiable. So the mechanism stays and the hierarchy goes.
 *
 * Neither weight is a hard constraint. The solver minimises weighted violation,
 * so two relations at the same weight that cannot both hold still produce a
 * compromise. DuckWM reaches the same conclusion with 1e8/1e4/1e2/1 and treats
 * its own REQUIRED as a large weight the fit honours rather than a separate
 * code path; generaldesign.md section 7 makes every constraint soft, and a
 * program that cannot be satisfied degrades rather than fails. The two values
 * are a decade apart, and that gap separates the two families from each other
 * and nothing finer: when a program relation and a client relation collide, the
 * program's wins the compromise, because the program is the arrangement the
 * client is being placed into rather than a claim competing with it.
 *
 * The weight does not reach inside a family. A snap against a named neighbour and
 * a snap against an output edge are both client-family relations, so they carry
 * the same 6000 and neither beats the other; the solve splits the disagreement
 * between them. An earlier version of this comment claimed the decade separated
 * those two cases, which is not something one number per family can do. Making
 * one win is the user's job rather than the solver's, and it is done by writing
 * the rule the user meant: a client carrying both a neighbour snap and an output
 * snap is asking for two incompatible positions, and the compromise is the
 * honest answer to that rather than a silently discarded half.
 *
 * Not part of the format. No program can write either value, and neither is a
 * name a config file may use. */
#define OMNI_SOLVER_WEIGHT_PROGRAM UINT16_C(60000)
#define OMNI_SOLVER_WEIGHT_CLIENT_RULE UINT16_C(6000)

/* --- client_rule (0x33) ---------------------------------------------- *
 *
 * OMNI_TAG_RULE matches a key and replaces a value. These match nothing and
 * solve a set of soft equations against a reference, so they are a different
 * record rather than a version field on one.
 *
 * One record is one rule of one set, and the fields are the keys of
 * layoutlanguage.md section 11.11. There is no payload: every argument is an
 * enum or an integer, so a fixed 24 bytes holds all of them and a rule that
 * omits `region` leaves it zero rather than carrying an inline value.
 *
 * A set is reached by name, never by matching, which is what removed most of
 * this record. There is no appid, no title and no title-regex flag, because
 * nothing in the language matches a window by property: a `clients` set is
 * reached by the action that creates the surface and the set is the surface's
 * identity, and a `snaps` set is reached by a drag region or a keybind naming
 * it. There is no target, for the same reason. There is no scope either, since
 * a set is resolved when it is reached and the language states no second
 * timing; a reload is the store replaying the config, not a rule re-resolving.
 *
 * There is no effect field because the namespace carries it: a set under
 * omniwm.clients arranges a surface and a set under omniwm.snaps names a
 * destination. An enum repeating that would let the two disagree. */

#define OMNI_CLIENT_RULE_OFF_RULE UINT32_C(0)    /* u16 */
#define OMNI_CLIENT_RULE_OFF_AGAINST UINT32_C(2) /* u16 */
#define OMNI_CLIENT_RULE_OFF_AXIS UINT32_C(4)    /* u16 */
#define OMNI_CLIENT_RULE_OFF_EDGE UINT32_C(6)    /* u16 */
#define OMNI_CLIENT_RULE_OFF_REGION UINT32_C(8)  /* u16 */
#define OMNI_CLIENT_RULE_OFF_RESERVED UINT32_C(10) /* u16, zero */
#define OMNI_CLIENT_RULE_OFF_WIDTH UINT32_C(12)  /* u32, pixels */
#define OMNI_CLIENT_RULE_OFF_HEIGHT UINT32_C(16) /* u32, pixels */
#define OMNI_CLIENT_RULE_OFF_BY UINT32_C(20)     /* i32, pixels, may be negative */
#define OMNI_CLIENT_RULE_SIZE UINT32_C(24)

#define OMNI_CLIENT_RULE_ALIGN UINT16_C(0)
#define OMNI_CLIENT_RULE_MATCH UINT16_C(1)
#define OMNI_CLIENT_RULE_SIZE_RULE UINT16_C(2)
#define OMNI_CLIENT_RULE_OFFSET UINT16_C(3)
#define OMNI_CLIENT_RULE_SNAP UINT16_C(4)
#define OMNI_CLIENT_RULE_KIND_COUNT UINT16_C(5)

#define OMNI_CLIENT_RULE_AGAINST_CLIENT UINT16_C(0)
#define OMNI_CLIENT_RULE_AGAINST_VIEWPORT UINT16_C(1)
#define OMNI_CLIENT_RULE_AGAINST_OUTPUT UINT16_C(2)
#define OMNI_CLIENT_RULE_AGAINST_COUNT UINT16_C(3)

#define OMNI_CLIENT_RULE_AXIS_X UINT16_C(0)
#define OMNI_CLIENT_RULE_AXIS_Y UINT16_C(1)
#define OMNI_CLIENT_RULE_AXIS_COUNT UINT16_C(2)

#define OMNI_CLIENT_RULE_EDGE_LEFT UINT16_C(0)
#define OMNI_CLIENT_RULE_EDGE_RIGHT UINT16_C(1)
#define OMNI_CLIENT_RULE_EDGE_TOP UINT16_C(2)
#define OMNI_CLIENT_RULE_EDGE_BOTTOM UINT16_C(3)
#define OMNI_CLIENT_RULE_EDGE_CENTER_X UINT16_C(4)
#define OMNI_CLIENT_RULE_EDGE_CENTER_Y UINT16_C(5)
#define OMNI_CLIENT_RULE_EDGE_COUNT UINT16_C(6)

#define OMNI_CLIENT_RULE_REGION_LEFT UINT16_C(0)
#define OMNI_CLIENT_RULE_REGION_RIGHT UINT16_C(1)
#define OMNI_CLIENT_RULE_REGION_TOP UINT16_C(2)
#define OMNI_CLIENT_RULE_REGION_BOTTOM UINT16_C(3)
#define OMNI_CLIENT_RULE_REGION_TOP_LEFT UINT16_C(4)
#define OMNI_CLIENT_RULE_REGION_TOP_RIGHT UINT16_C(5)
#define OMNI_CLIENT_RULE_REGION_BOTTOM_LEFT UINT16_C(6)
#define OMNI_CLIENT_RULE_REGION_BOTTOM_RIGHT UINT16_C(7)
#define OMNI_CLIENT_RULE_REGION_CENTER UINT16_C(8)
#define OMNI_CLIENT_RULE_REGION_FULL UINT16_C(9)
#define OMNI_CLIENT_RULE_REGION_COUNT UINT16_C(10)

OMNI_STATIC_ASSERT(OMNI_CLIENT_RULE_OFF_BY + 4 == OMNI_CLIENT_RULE_SIZE,
	"client rule record");
OMNI_STATIC_ASSERT(OMNI_CLIENT_RULE_OFF_RESERVED + 2 <= OMNI_CLIENT_RULE_OFF_WIDTH,
	"reserved precedes the 4-aligned fields");
OMNI_STATIC_ASSERT(OMNI_CLIENT_RULE_SIZE % 4 == 0, "client rule 4-aligned");

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
#define OMNI_BINDING_OFF_ACTION_REF UINT32_C(12) /* u32 frame offset of the action's name */
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
 * and what it is placed against is in the program, not here.
 *
 * The stage byte is what tells a reader whether the extents it is reading are the
 * solver's answer or the activated one. The pipeline in layoutengine.md 3.7
 * writes twice per pass, once solved and once arranged, and the two are
 * different values rather than two names for one: relaxation runs between the
 * solve and the write, so the solved write is the solver's output plus
 * magnetisation, and the arranged write is that same value once the animate
 * step has completed. A reader that cannot tell them apart cannot tell whether
 * it is looking at the endpoint of the current pass or at an intermediate that
 * the animator is about to move away from, which is the difference between
 * "apply this now" and "wait".
 *
 * One buffer serves both stages rather than two buffers, and the reason is that
 * the two are not concurrent. Step 6 writes SOLVED and step 9 overwrites it with
 * ARRANGED, and a reader that samples between them gets SOLVED with a generation
 * that no longer matches the block's current commit, which is the same
 * revalidation every other section already does. Two buffers would cost a second
 * 32KB section and a second capability bit to answer a question that the stage
 * byte plus the existing generation check already answers. What a reader cannot
 * do is retain the solved geometry after the arrange has overwritten it, and
 * nothing needs to: the animator is handed the endpoint, not the intermediate. */

#define OMNI_SOLVED_OFF_GENERATION UINT32_C(0) /* u64, bumped once per write */
#define OMNI_SOLVED_OFF_NODE_COUNT UINT32_C(8) /* u32 */
#define OMNI_SOLVED_OFF_STAGE UINT32_C(12)     /* u8 */
#define OMNI_SOLVED_OFF_RESERVED UINT32_C(13)  /* u8 x 3, zero */
#define OMNI_SOLVED_OFF_NODES UINT32_C(16)

/* Stage of the extents currently in the section. */
#define OMNI_SOLVED_STAGE_SOLVED UINT8_C(0)   /* solver output, post-relaxation */
#define OMNI_SOLVED_STAGE_ARRANGED UINT8_C(1) /* activated; the animate step is done */
#define OMNI_SOLVED_STAGE_COUNT UINT8_C(2)
#define OMNI_SOLVED_NODE_OFF_REF_ID UINT32_C(0)
#define OMNI_SOLVED_NODE_OFF_REF_GEN UINT32_C(4)
#define OMNI_SOLVED_NODE_OFF_X UINT32_C(8)  /* i32, canvas coordinates */
#define OMNI_SOLVED_NODE_OFF_Y UINT32_C(12) /* i32 */
#define OMNI_SOLVED_NODE_OFF_W UINT32_C(16) /* i32 */
#define OMNI_SOLVED_NODE_OFF_H UINT32_C(20) /* i32 */

OMNI_STATIC_ASSERT(OMNI_SOLVED_OFF_RESERVED + 3 == OMNI_SOLVED_OFF_NODES,
	"solved header");
OMNI_STATIC_ASSERT(OMNI_SOLVED_NODE_OFF_H + 4 == OMNI_SOLVED_NODE_SIZE,
	"solved node record");
OMNI_STATIC_ASSERT(OMNI_SOLVED_OFF_NODES % 4 == 0, "solved nodes aligned");

/* --- layout names ------------------------------------------------------ *
 *
 * This replaces a 24-slot table of Greek letters, which was closed for a reason
 * that no longer applies: it was closed because a slot is an address with no
 * meaning of its own, and a user-writable name is the opposite of that. Nothing
 * here is closed, and nothing scales with the length of a list.
 */

/* The built-in layouts shipped at v1, and the seed the compositor registers at
 * startup. A user names a layout freely and a user name may not equal a
 * registered built-in, so there is no precedence rule and no reserved list to
 * keep complete: the guard reads the names the compositor actually registered,
 * not this string, so a built-in that is implemented without appearing here is
 * still reserved and still cannot be shadowed.
 *
 * The seed is here because a constant needs one home and this file is it, and
 * that is the only reason. Adding a built-in is a registration, not an ABI edit,
 * so the number of built-ins is not a design quantity: ten cost ten registrations
 * and no change to any table, any guard, or any format version. The list is a
 * convenience for the reader and a starting point for the implementation, not
 * the reserved set.
 *
 * Names are compared exactly, as written in devnotes/layoutlanguage.md, which is
 * why every name here is lowercase with underscores and none contains a hyphen.
 */
#define OMNI_LAYOUT_BUILTIN_NAMES \
	"monocle", "dwindle", "vertical_stack", "master_stack"

/* The depth a group naming a nested layout may reach, checked when a program is
 * loaded and never while solving, so a rejected program leaves the running
 * layout untouched. Acyclicity is implied by a depth cap rather than checked
 * separately: `master_stack` naming `master_stack` nests to the cap and is
 * refused there, where the old slot table could have cascaded forever and hung
 * the compositor instead of producing a visible mistake. Five sits at the point past which
 * nesting is not readable on screen, and it is what makes the arrange pass's
 * cost bounded by a constant factor rather than merely safe. Free naming does
 * not weaken it, since the bound is on depth and not on how many layouts
 * exist. */
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
/* The set of bindings changed. Distinct from KEY_SET because a binding change
 * invalidates a *derived* structure, not one cached value: a process-private
 * binding index must be rebuilt, and a client that only cares that "the binds
 * are not what I read" needs one event rather than one per binding. */
#define OMNI_JOURNAL_KIND_BINDS_UPDATED UINT8_C(5)

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
#define OMNI_REQ_OFF_NAME_REF UINT32_C(56)      /* u32 arena frame offset */
#define OMNI_REQ_OFF_RESERVED_NAME_PAD UINT32_C(60) /* u32, zero */
#define OMNI_REQ_OFF_VALUE UINT32_C(64) /* 128 B */
#define OMNI_REQ_VALUE_FIELD_SIZE UINT32_C(128)
#define OMNI_REQ_OFF_REQUESTER_START_ID UINT32_C(192) /* u32 */
#define OMNI_REQ_OFF_RESERVED_PAD UINT32_C(196) /* u32, zero; 8-aligns the next field */
#define OMNI_REQ_OFF_TERMINAL_AT_MS UINT32_C(200)     /* u64 */
#define OMNI_REQ_RESERVED_TAIL_START UINT32_C(208)

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

/* OMNI_REQ_OFF_RESULT_CODE holds an OMNI_ERR_* value from section 12. There is
 * no separate success constant: OMNI_ERR_NONE is the success value, so a slot
 * carries exactly one vocabulary and there is no second "OK" that can disagree
 * with it. Status and result cannot contradict each other, because DONE is
 * defined as status DONE with OMNI_ERR_NONE and ERROR as status ERROR with any
 * other code. */

OMNI_STATIC_ASSERT(OMNI_REQ_OFF_NAME_REF + 4 == OMNI_REQ_OFF_RESERVED_NAME_PAD,
	"name ref field");
OMNI_STATIC_ASSERT(OMNI_REQ_OFF_RESERVED_NAME_PAD + 4 == OMNI_REQ_OFF_VALUE,
	"name pad field");
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

/* ------------------------------------------------------------------ */
/* 11. Socket facade (ipc.md sections 1 and 3.3)                        */
/* ------------------------------------------------------------------ */

/* ipc.md section 1. NDJSON framing, one request per line. */
#define OMNI_SOCK_BACKLOG UINT32_C(16)
#define OMNI_SOCK_MAX_LINE (UINT32_C(1) * 1024 * 1024)
#define OMNI_SOCK_SEND_BUF (UINT32_C(64) * 1024) /* batching threshold      */
#define OMNI_SOCK_STALL_MS UINT32_C(2000)         /* no forward progress     */
#define OMNI_SOCK_DISCONNECT_MS UINT32_C(60000)   /* still stalled after the
                                                       partial teardown */
/* Bytes in sockaddr_un.sun_path, NUL included. 107 on Linux, 103 on the BSDs
 * and on macOS, so a path is truncated rather than refused on those. */
#define OMNI_SOCK_SUN_PATH_MAX UINT32_C(107)

/* ipc.md section 3.3. Every decoded quantity is bounded before any arena
 * allocation, so the store is never asked to hold a frame larger than the
 * arena. The bases are in section 2 above; these are the numbers derived from
 * them, and a reader changing one of those has to recheck this table. */
#define OMNI_VALUE_MAX_NAME UINT32_C(4095) /* == OMNI_REQUEST_NAME_MAX      */
#define OMNI_VALUE_MAX_STRING (UINT32_C(1) * 1024 * 1024)
#define OMNI_VALUE_MAX_BLOB (UINT32_C(768) * 1024)
#define OMNI_VALUE_MAX_FRAMED (UINT32_C(1) * 1024 * 1024)
#define OMNI_VALUE_MAX_ARRAY_ELEMS UINT32_C(65536)
#define OMNI_VALUE_MAX_TUPLE_FIELDS UINT32_C(64)
#define OMNI_VALUE_MAX_GROUPED_KEYS UINT32_C(4096) /* == journal capacity    */
#define OMNI_VALUE_MAX_NESTING UINT32_C(16)

/* ipc.md section 3, and tomlparser.md section 8. A config file's operations
 * are staged and then published as one grouped commit, so this bounds the
 * staging and not the journal. */
#define OMNI_CONFIG_MAX_OPS UINT32_C(4096)

OMNI_STATIC_ASSERT(OMNI_VALUE_MAX_NAME == OMNI_REQUEST_NAME_MAX, "name bound");
OMNI_STATIC_ASSERT(OMNI_VALUE_MAX_GROUPED_KEYS == OMNI_JOURNAL_CAPACITY,
	"one journal ring of entries per grouped commit");
OMNI_STATIC_ASSERT(OMNI_SOCK_SUN_PATH_MAX <= OMNI_SOCK_MAX_LINE, "path fits a line");

/* ------------------------------------------------------------------ */
/* 12. Error vocabulary (configstorelayout.md 6.1, 9, 14; ipc.md 2)    */
/* ------------------------------------------------------------------ */

/* One vocabulary, three surfaces. A direct read, a queued request, and a socket
 * request all return an OMNI_ERR_* value, because all three are answers to the
 * same two questions: did the store do what was asked, and if not, which fact
 * stopped it. Three surfaces used three vocabularies before this section, which
 * made every caller translate and left "the catalog index is unreadable"
 * expressible in a form that looked like "you passed a bad argument".
 *
 * The split inside this section is by *who can produce the code*, not by which
 * surface reports it. OMNI_ERR_* names a fact about the block, so the store
 * produces it and a direct reader can hit it without a request ever existing.
 * OMNI_SOCK_ERR_* names a fact about a message or a facade limit, so only the
 * socket can produce it and no direct read can ever return one. A code is in
 * exactly one of the two sets, which is what makes the split checkable. */
#define OMNI_ERR_NONE UINT8_C(0)
#define OMNI_ERR_PARAM_INVALID UINT8_C(1)
#define OMNI_ERR_NAME_TOO_LONG UINT8_C(2)
#define OMNI_ERR_BAD_TARGET UINT8_C(3)
#define OMNI_ERR_KEY_NOT_FOUND UINT8_C(4)
#define OMNI_ERR_ENTRY_FREE UINT8_C(5)
#define OMNI_ERR_GENERATION_MISMATCH UINT8_C(6)
#define OMNI_ERR_CATALOG_FULL UINT8_C(7)
#define OMNI_ERR_ARENA_FULL UINT8_C(8)
#define OMNI_ERR_REGION_FULL UINT8_C(9)
#define OMNI_ERR_REGION_TOO_LARGE UINT8_C(10)
#define OMNI_ERR_BLOCK_EXHAUSTED UINT8_C(11)
#define OMNI_ERR_BLOCK_TRUNCATED UINT8_C(12)
#define OMNI_ERR_BLOCK_UNSUPPORTED UINT8_C(13)
#define OMNI_ERR_STORE_BROKEN UINT8_C(14)
#define OMNI_ERR_NOT_READY UINT8_C(15)
#define OMNI_ERR_REQUEST_EXPIRED UINT8_C(16)
#define OMNI_ERR_VALUE_UNREADABLE UINT8_C(17)

/* The four read codes exist because a read is a plain memory access with no slot
 * to hold a result, and this set is what it returns instead:
 *
 *   KEY_NOT_FOUND        the name is not in the index. An absence, and a normal
 *                        one: the open catalog accepts writes of names nobody
 *                        has heard of, so this is how a read of such a name ends.
 *   BLOCK_UNSUPPORTED    the block is well formed and this build cannot read
 *                        it: a format or index version it does not implement, or
 *                        a mandatory section that is absent or malformed. A
 *                        reader never answers these slowly instead.
 *   BLOCK_TRUNCATED      the block is shorter than the layout, so a section the
 *                        reader needs is not there yet. Distinct from
 *                        UNSUPPORTED because it is a smaller block rather than
 *                        a different one, and it can grow.
 *   GENERATION_MISMATCH  the name resolved, but the entry moved under a held
 *                        (entry_id, entry_generation) reference. The reader may
 *                        retry by name; a caller holding the reference does not
 *                        get a torn value.
 *   ENTRY_FREE           the slot behind a held reference is on the freelist.
 *                        Reported rather than skipped, because a held reference
 *                        is not the open-catalog lookup case that skipping
 *                        serves.
 *
 * VALUE_UNREADABLE is the one code a guard refusal produces, and keeping it
 * separate from BLOCK_UNSUPPORTED is what makes the two failure scales legible.
 * BLOCK_* is about the block: a build that cannot read this shape, or a block
 * too small to hold it. VALUE_UNREADABLE is about one reference inside a block
 * this build understands perfectly: the slot index is out of range, the entry
 * moved under a held generation, the frame is on the free list, the declared
 * length disagrees with the frame header, or the value's shape contradicts the
 * tag. A reader that gets it has learned that this key cannot be decoded and that
 * the other keys in the same block are still worth reading, which is a different
 * decision from refusing the block. Collapsing the two would force a client to
 * rediscover the whole instance over one stale key reference.
 */

#define OMNI_SOCK_ERR_NONE UINT8_C(0)
#define OMNI_SOCK_ERR_INVALID_JSON UINT8_C(1)
#define OMNI_SOCK_ERR_UNKNOWN_CMD UINT8_C(2)
#define OMNI_SOCK_ERR_BAD_TYPE UINT8_C(3)
#define OMNI_SOCK_ERR_BAD_VALUE UINT8_C(4)
#define OMNI_SOCK_ERR_ARGS_INVALID UINT8_C(5)
#define OMNI_SOCK_ERR_ACTION_NOT_FOUND UINT8_C(6)
#define OMNI_SOCK_ERR_ACTION_FAILED UINT8_C(7)
#define OMNI_SOCK_ERR_WATCH_INVALID UINT8_C(8)
#define OMNI_SOCK_ERR_WATCH_GAP UINT8_C(9)
#define OMNI_SOCK_ERR_WATCH_EPOCH_CHANGED UINT8_C(10)
#define OMNI_SOCK_ERR_CONFIG_TOO_LARGE UINT8_C(11)

/* BAD_TYPE and BAD_VALUE are here and not in OMNI_ERR_* because they are
 * questions about what a client asked for, not about the block. The store
 * stores a value of whatever tag names it, which is the open-catalog rule, so
 * it has no opinion on whether a u32 is a sensible setting for something.
 * BAD_TYPE is a JSON shape that does not match the tag; BAD_VALUE is a matching
 * shape whose contents the consumer rejects. The store never produces either,
 * and a direct read never returns either, because a direct read has no client
 * message to be wrong about. A consumer that validates a value on read reports
 * its own failure in its own terms, and the guards in configstorelayout 12 are
 * deliberately not able to produce these two.
 *
 * WATCH_* are also not collapsible into a generic failure. WATCH_GAP is "you
 * missed events, resynchronise", WATCH_EPOCH_CHANGED is "the block you were
 * watching is a different block", and a client that cannot tell them apart
 * cannot decide whether to re-read one key or re-discover the instance.
 * CONFIG_TOO_LARGE is a facade limit: a staged config file exceeded
 * OMNI_CONFIG_MAX_OPS, which is a socket input bound and not a store capacity. */

/* The wire name of a code is the constant's name with the OMNI_ERR_ or
 * OMNI_SOCK_ERR_ prefix removed, verbatim: OMNI_ERR_NOT_READY is the string
 * NOT_READY, OMNI_SOCK_ERR_WATCH_GAP is WATCH_GAP. A socket response therefore
 * carries either an OMNI_ERR_* name or an OMNI_SOCK_ERR_* name and never a
 * third spelling, and a client can map any string it sees to a constant with a
 * prefix it already knows. ipc.md section 2 lists the resulting closed set.
 *
 * TOML has no codes and is not a surface here. A config file is parsed before
 * anything is published, so its failures are parse diagnostics carrying a line
 * and a column, reported to whoever loaded the file, and they never reach a
 * client that would have to interpret a code. tomlparser.md section 9 lists the
 * diagnostics instead. */

/* Both sets are u8 because the request slot stores a result code in one byte, so
 * the vocabulary is bounded by the slot and not by a wider type a future caller
 * might assume. Adding a code past 255 means widening the slot, which changes
 * the section size and every offset after it. */
OMNI_STATIC_ASSERT(OMNI_ERR_VALUE_UNREADABLE <= UINT8_MAX &&
                   OMNI_ERR_VALUE_UNREADABLE == UINT8_C(17),
	"core code count is the highest core code, and both fit the result byte");
OMNI_STATIC_ASSERT(OMNI_SOCK_ERR_CONFIG_TOO_LARGE <= UINT8_MAX &&
                   OMNI_SOCK_ERR_CONFIG_TOO_LARGE == UINT8_C(11),
	"socket code count is the highest socket code, and both fit the result byte");
OMNI_STATIC_ASSERT(OMNI_REQ_OFF_RESULT_CODE + 1 == OMNI_REQ_OFF_FLAGS,
	"the result code byte is followed by the flags byte");

#endif /* OMNIWM_SHARED_OMNI_LAYOUT_H */

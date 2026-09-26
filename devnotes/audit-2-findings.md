the report *did* get made, but the model compacted immediately afterwards, here's the report:

omniWM design audit — stages 1 to 4
Scope note first: include/shared/omni_layout.h is the only non-empty file in include/ and src/. All 59 .c files, all 59 .h files, and ipc/ipc.c, ipc/ipc.h are 0 bytes. There is no build system. devnotes/ contains 16 files; layout-test-examples.md is not in your list but is 1138 lines and is referenced normatively. layoutsystem.md does not exist; looks.md exists and is 0 bytes.
STAGE 1 — config store and ipc functional, simple write+read test
Required documents
Per generaldesign.md:466 → configstorage.md, ipc.md, testing.md. Per missing-devnotes-topics.md:30 → testing.md (stage 1, P0). configstorelayout.md is required transitively: configstorage.md:108 "The exact sizes and offsets are in configstorelayout.md §2."
doc
configstorage.md
configstorelayout.md
ipc.md
testing.md
1.1 (a BLOCKER) configstorelayout.md §3 contradicts its own header table
The prose says the header fields end at 0x087 and 0x088..0x0FF is reserved. The table on the very next lines places twelve fields inside that range.
- configstorelayout.md:138 — "0x000000  header fields: 0x000..0x087; reserved through 0x0FF"
- configstorelayout.md:152 — "## 3. Header (offset 0, fields 0x000..0x087, region 0x300)"
- configstorelayout.md:191 — "Fields from 0x088 through 0x0FF are reserved and must be zero on creation."
- vs. the table: configstorelayout.md:172 | 88 | 8 | region_head | … configstorelayout.md:187 | 160 | 4 | ready |, and configstorelayout.md:188 | 164 | 92 | reserved | zero through offset 255 |
An implementer following the prose leaves offsets 88–163 uninitialised. The header agrees with the table, not the prose: omni_layout.h:271 OMNI_HDR_RESERVED_TAIL_START UINT32_C(164) /* through offset 255 */. The prose is the stale half.
1.2 (a BLOCKER) configstorage.md §4 contains ~25 lines of stale text describing the replaced constraint record
configstorage.md:241-252 correctly states the constraint payload is a tree. Immediately after, an orphaned code-fence fragment and a full paragraph describe the deleted 32-byte packed record:
- configstorage.md:268 — "12   4     literal      i32"  (a stray fragment of the old record table, sitting after the code fence already closed at line 252)
- configstorage.md:278-287 — "priority is a weight, not a rank, and the four bands (DOMINANT 60000, STRONG 6000, MEDIUM 600, WEAK 60) … The edges bit positions are the same four river's set_tiled and Hyprland's Layout::eRectCorner use … Subject and operand are entry_id plus entry_generation … A length that is not a multiple of 32 is malformed."
Contradicted by configstorage.md:256-261 ("kind became the rule key of the language and the record's flags, edges, axis and role went with it … priority is gone … role, and with it ROLE_PARENT, went"), by omni_layout.h:477-481 ("The record also had a priority, and its absence below is not an oversight. There is no priority in the language"), and by omni_layout.h:456-462 (constraint := array of space). A reader implementing the constraint payload from §4 gets two mutually exclusive definitions in the same section.
1.3 (a BLOCKER) configstorage.md:614 known-tag range stops at 0x33; the tag 0x34 falls in no branch of the three-way tag test
- configstorage.md:614 — "0x01..0x33        known:      require the tag's exact encoded length…"
- configstorage.md:618 — "0x00, 0x35..0x7FFF unassigned: malformed, refused by the core"
- configstorage.md:627-628 — "0x34 for the window-rule block, and the hole simply moved from 0x32..0x7FFF to 0x35..0x7FFF"
- omni_layout.h:438 — OMNI_TAG_MAX_KNOWN UINT16_C(0x34)
- omni_layout.h:441 — OMNI_TAG_UNASSIGNED_LO UINT16_C(0x0035)
- configstorelayout.md:762 — "L4 an unassigned low tag (0x00, 0x35..0x7FFF) is refused, not read as an extension"
0x34 (OMNI_TAG_MAP) is in no branch. A tag-decision function written from §12.1 has no case for it and cannot classify a window rule. Line 614 is the single wrong number; lines 618, 627-628, the header, and configstorelayout.md:762 all agree it should be 0x01..0x34.
1.4 (a BLOCKER) ipc.md has no JSON encoding for tag 0x34 (map) while asserting it has one for every tag
- ipc.md:298 — "Every type named in the storage table of configstorage.md §4 has a row here."
- The §3 table (ipc.md:266-296) has rows for every tag except map. grep -n "\bmap\b" ipc.md returns nothing.
- configstorage.md:344 — "A window rule is a map with two well-known keys, if and then, and it gets no tag of its own."
- ipc.md:943 — "A rule is a block value, so it is reached by the ordinary key verbs"
A window rule — the stage-5/8 feature ipc.md itself makes a stage-1 concern at ipc.md:932-942 — has no defined wire form.
1.5 (a BLOCKER) action_ref means two different things in two documents
- configstorage.md:227 — "12   4     action_ref   u32 arena frame offset, the action name"
- omni_layout.h:586 — #define OMNI_BINDING_OFF_ACTION_REF UINT32_C(12) /* u32 frame offset */
- helpers.md:419 — "action_ref in the block is the u32 handle, and it is resolved through the same registry lookup…"
- ipc.md:353 — "A binding names its action on the wire and stores the resolved handle, so a binding whose action is not registered is ACTION_NOT_FOUND at set time rather than a dangling handle stored for later."
Two docs say frame offset, two say handle. Same field, same 4 bytes, incompatible semantics. A reader written from the store side dereferences a frame; a reader written from the helpers side indexes a registry.
1.6 (a BLOCKER) ipc.md §5.2 vs server.md §4: is delete served while NOT_READY?
- ipc.md:885 — "| unwatch, delete | served |"
- server.md:85-86 — "Reads are still served in every one of these states, and writes are refused with NOT_READY until ready is READY (ipc.md §5.2)."
delete is a write (ipc.md:749 "The entry goes DESTROYED + FREE"). ipc.md §5.2 also serves save (a read) and refuses set/exec/reload — the split is otherwise coherent, so ipc.md:885 is the outlier, but the two normative statements disagree.
1.7 (a BLOCKER) ipc.md §1 and §3.3 declare constants that do not exist in the single definition point
ipc.md:29 — "Constants (single definition point, include/shared/omni_layout.h):" then lists OMNI_SOCK_MAX_LINE, OMNI_SOCK_BACKLOG, OMNI_SOCK_SEND_BUF, OMNI_SOCK_STALL_MS, OMNI_SOCK_DISCONNECT_MS, OMNI_SOCK_SUN_PATH_MAX (ipc.md:32-37).
ipc.md:386-394 lists OMNI_VALUE_MAX_NAME, OMNI_VALUE_MAX_STRING, OMNI_VALUE_MAX_BLOB, OMNI_VALUE_MAX_FRAMED, OMNI_VALUE_MAX_ARRAY_ELEMS, OMNI_VALUE_MAX_TUPLE_FIELDS, OMNI_VALUE_MAX_GROUPED_KEYS, OMNI_VALUE_MAX_NESTING.
configstorage.md:718 and ipc.md:719 and tomlparser.md:228 use OMNI_CONFIG_MAX_OPS.
grep -n "OMNI_SOCK\|OMNI_VALUE\|OMNI_CONFIG" include/shared/omni_layout.h → no match. 15 constants that three documents say live in the one file that declares itself the only home (filestructure.md:53 "No constant is duplicated anywhere else, and changing a value must be a one-line edit here"; configstorelayout.md:863 "include/shared/omni_layout.h is the only home for every constant in §2") do not exist in it.
1.8 (a BLOCKER) ipc.md mis-cites three sections of the document it depends on
- ipc.md:6 — "the commit protocol (§12 of configstorelayout.md)" — the commit protocol is configstorelayout.md:780 "## 13. Commit protocol, concrete". §12 is "## 12. Invariants".
- ipc.md:484 — "Mapped onto commit protocol §12 of configstorelayout.md" — same error.
- ipc.md:461 — "A structurally invalid entry (configstorelayout.md §12 guard bundle) is reported as BAD_VALUE" — there is no "guard bundle"; configstorage.md:573-574 says "This replaces the single flat bundle that could not be evaluated as written," and the tiers live in configstorage.md §12, not configstorelayout.md §12.
1.9 (b DOCUMENTED DEFERRAL, acceptable) configstorage.md §14 "Still deferred"
- configstorage.md:745 — "Multi-painter arbitration on a single region (out of scope: one painter per region)."
- configstorage.md:746 — "Value transactions beyond grouped commits (no CAS at v1)."
- configstorage.md:747 — "Mixed-endianness hosts (native, tied to format_version)."
- configstorage.md:748 — "Extension-specific semantic validation beyond structural framing checks."
- configstorage.md:749 — "Compile-time ABI tests asserting the header's values against this document."
The last one is explicitly a build-time test, not a design gap; the header already carries 30 OMNI_STATIC_ASSERTs. The first four do not block a write+read test.
configstorelayout.md:849-862 §14 "Open items" is a list of v1 capacity choices with stated rationales, not unresolved questions.
1.10 (b DOCUMENTED DEFERRAL, but with a caveat) the per-record semantic guards are recorded as not done
- layoutengine.md:1940 — "| configstorage.md | §12 guards | a semantic guard per record, and a key-path rule that an omniwm.layouts.<name> component is checked against the closed set | partial, the tag-range guard is done, the record guards are not |"
- layoutlanguage.md:723-724 — "Load-time validation of a program, all in the semantic guard of devnotes/configstorage.md §12" — followed by 13 numbered program validations (layoutlanguage.md:726-747) and 6 client ones (layoutlanguage.md:753-761).
- configstorage.md:348-353 — "The cost of no dedicated tag is that nothing structural stops if from holding a non-string or then from holding a read-only field, so a rule's validation is semantic rather than framing-level, and it lands in the bucket §14 already defers."
So 19 named validations are assigned to configstorage.md §12 by one document, recorded as undone by another, and deferred by a third. Deferral is documented, but three documents give three different accounts of where it lives. It does not block a write+read test.
1.11 configstorelayout.md is not self-sufficient for a reader/writer
You asked specifically. Answer: the fixed structures are complete and byte-exact — §3 header, §4 section table, §5 arena frame, §6 catalog entry, §7 growth, §8 journal ring (slot layout matches omni_layout.h:667-684 field-for-field, 64 bytes exactly), §9 request queue (4352 = 0x1100 ✓), §10 region descriptors, §11 solved layout, §12 the 34-row invariant table, §13 the commit protocol with a worked example. That part is implementable as written.
What is not in it: the byte tables for the three composite payloads reopened at configstorelayout.md:16-19 ("the tag table, the composite payload shapes, the fixed section set, and the v1 capacity constants"). grep -n "action_ref\|args_ref\|OMNI_BINDING\|OMNI_CLIENT_RULE\|OMNI_LAYOUT_MAX_NEST_DEPTH" configstorelayout.md → no match. Those three records exist only in configstorage.md §4 (configstorage.md:220-231 binding, configstorage.md:292-303 client_rule, configstorage.md:247-252 constraint) and in the header's comments — even though omni_layout.h:57-58 names "devnotes/configstorelayout.md sections 2-10 (byte layout, commit protocol)" as the normative reference, and configstorage.md:214-216 says the numbers are "numbered in omni_layout.h". The layout document, the document that owns byte layout, has no entry for them.
1.12 Numeric values that could disagree, side by side
quantity
request slots
request error codes
known-tag range
header fields end
OMNI_CAP_DEFAULT
initial pool
known-tag range (2nd source)
All §2 constants otherwise reconcile exactly with the header (checked every row, :56-93 vs omni_layout.h:109-168; derived values at :97-122 all reproduce; all five static asserts pass by hand).
1.13 Stale statements that mislead a stage-1 implementer
- layoutengine.md:343 — "§2.10 is the list of what the layout engine needs from that surface, with the cost of each. Nothing in it has been applied." Contradicted by layoutengine.md:369 ("all three now resolved"), :444 ("Taken: the fixed section"), :1938-1948 (state column "done"), configstorelayout.md:20-23 ("That list has been applied here"), and omni_layout.h:10-12 ("That list has now been applied"). Following this line would mean re-deriving the entire tag table.
- layoutlanguage.md:770-773 — "That reverses devnotes/layoutengine.md §2.10's claim that a record is 16 bytes … the header's OMNI_TAG_CONSTRAINT block does not describe this language and would need to become a composite-value tag rather than a record array." It already is one: omni_layout.h:451-462.
- configstorage.md:88-103 — the §2 "Block layout" diagram lists header, catalog, journal, requests, region desc, pool. The solved-layout section is absent. configstorelayout.md:144 and omni_layout.h:310 both have it as fixed section 7.
- architecture-audit.md:33-45 — "All 59 .c files and all 59 .h files under src/ and include/ were empty"; "devnotes/server.md, and devnotes/tomlparser.md are empty"; "include/shared/omni_layout.h … does not exist". All four are now false. The document labels itself a historical record at :7-13, so this is expected — but it cannot be used as a readiness gate. Its §8 "Out of scope, deferred" table (:963-970) is the load-bearing part.
- ipc.md:961-965 — "binding and rule wire encodings are unfinished by design … the runtime failure code for an action that fails during execution, none of which are defined yet." All four are defined in helpers.md §6.1-§6.2. Stale open item; see 3.4.
STAGE 2 — TOML parser for config store + helper for registering new TOML keys
Required documents
generaldesign.md:467 → tomlparser.md only. Exists, 278 lines. But the "helper for registering new TOML keys" half of the stage is not in tomlparser.md; it is in helpers.md (stage 3).
2.1 (a BLOCKER) tomlparser.md §2 invents three type tags that exist in no tag table and no header
- tomlparser.md:60-66 — "| a binding table | binding | … | a client rule table | client_rule | … | a layout program | layout | rules and spaces are arrays of tables, viewport is a table | | a layout rule table | layout_rule | rule plus that kind's own arguments | | a space table | space | name/layout/x/y/w/h/order |"
- tomlparser.md:11 — "It adds no tags of its own"
- configstorage.md:194-196 — the tags are 0x32 constraint, 0x33 client_rule, 0x34 map. There is no layout, no layout_rule, no space.
- omni_layout.h:432-434 — OMNI_TAG_CONSTRAINT 0x32, OMNI_TAG_CLIENT_RULE 0x33, OMNI_TAG_MAP 0x34. grep -rn "layout_rule" . ../include/ → tomlparser.md:65 only.
- configstorage.md:248-252 and omni_layout.h:456-459 — a space is an option (0x25) inside the constraint tree, and a rule is a named key of that option. There is no separate tag for either.
tomlparser.md:64,65,66 are the only occurrences of three tag names in the entire repository. A TOML table for a layout program has no tag to bind to.
2.2 (a BLOCKER) Three-way disagreement on what a layout program's tag is called, and who owns it
- layoutlanguage.md:21 — "One composite value bound to the constraint tag of devnotes/tomlparser.md §2, at omni.layouts.<name>" — i.e. layoutlanguage defers the tag definition to tomlparser §2.
- tomlparser.md:64 — tomlparser.md §2 calls that tag layout.
- configstorage.md:194 / omni_layout.h:432 — the tag is constraint (0x32).
layoutlanguage.md §2 cites tomlparser.md as the authority; tomlparser.md names the tag differently from the store.
2.3 (a BLOCKER) One key or three keys for a layout program — configstorage.md and tomlparser.md disagree
- configstorage.md:263-264 — "A program is therefore a value like any other, which is what layoutengine.md §4.6 asks for: save writes the tree and a user can edit it"
- configstorage.md:728 — "a layout authored over IPC at omniwm.layouts.delta is written by save omniwm.layouts.delta.*"
- layoutengine.md:1099-1101 — "omniwm.layouts.master_stack = <program>", "omniwm.layouts.sidebar = <program>", "omniwm.layouts.monocle" — one key per program
- layoutlanguage.md:44 — "| program | a named layout, one entry of omniwm.layouts, holding one rule list | omniwm.layouts.<name> |"
- tomlparser.md:76 — "So a whole program is three keys and nothing else", followed by [[omniwm.layouts.delta.rules]] ×2 and [[omniwm.layouts.delta.spaces]] ×2 (:79-94)
- layoutlanguage.md:579-592 — its own examples use [[omniwm.layouts.three_sides.rules]] and [[omniwm.layouts.three_sides.spaces]]
- layout-test-examples.md — 1138 lines, every program in the suite is in three-key form (:35-52, :73-88, :165-169, …)
- layoutlanguage.md:811 — "so the only omniwm.layouts.<name>.* key that makes one is the space's layout"
configstorage.md/layoutengine.md/layoutlanguage.md §2 say one key. tomlparser.md, layoutlanguage.md's own examples, and the whole 1138-line test suite say three. layoutlanguage.md contradicts itself between :44 and :579. This determines the save scope grammar, the key-path guard, and the TOML reader's flattening rule — all stage-1/2 code.
2.4 (a BLOCKER) tomlparser.md is contradicted by ipc.md on the binding's action field name
- ipc.md:292 — "| binding | object {"mods","key","cmd","args"?} |"
- helpers.md:407-410 — { "type": "binding", "value": { "mods": "ctrl+alt", "key": "Shift+Return", "action": "wm.cycle_layout", "args": ["master_stack"] } }
- ipc.md:652 — its own exec example uses "action": { "cmd": "exec", "action": "wm.cycle_layout", ... }
ipc.md §3 says cmd; ipc.md §4 exec and helpers.md §6.2 both say action. Same wire object, two field names.
2.5 Is tomlparser.md otherwise complete?
Largely yes. §1 config-file model, §3 numeric typing from key registration, §4 the two suffixed forms, §5 arrays + elem_type, §6 tables→tuple, §7 datetimes, §8 staging + grouped commit, §9 save round-trip, §10 facade relationship, §11 open items. Three small items:
- (b deferral) tomlparser.md:265-269 — "The full TOML grammar is not restated here … the specification itself is upstream and the parser is expected to conform to it". Acceptable, explicitly scoped.
- (b deferral) tomlparser.md:275-278 — "Arithmetic on datetime values … is not in scope here." Acceptable.
- (reference) tomlparser.md:113 — "A component declares each option's tag in its descriptor (helpers.md §3.1)" — resolves, but the mechanism is helpers.md:178-182 struct omni_option, which stage 3 must deliver. Cross-stage dependency, not a defect.
- (dangling) tomlparser.md:251 — "The parser is a facade in exactly the sense configstorage.md §11 and §12 describe" — §12 is guard rules, not facade semantics (§11 is). Cosmetic.
- (inconsistency) tomlparser.md:63 binds a "client rule table" to client_rule; configstorage.md:344-348 says a window rule is a map and windows.md:323 puts window rules at omniwm.window_rules.<name>. These are different objects (a 0x33 constraint inside a set vs. a window rule), and the documents do keep them apart — but tomlparser.md provides no binding for a window-rule map table at all, which compounds 1.4.
STAGE 3 — shared helpers, core libraries, logging, server backend
Required documents
generaldesign.md:468 → helpers.md, build.md, licence.md. Per missing-devnotes-topics.md:29 → build.md (stage 3, P0); :31 → licence.md (stage 3, P3).
doc
helpers.md
server.md
build.md
licence.md
3.1 (a BLOCKER) build.md does not exist and stage 3 cannot produce a build
- generaldesign.md:468 — "| 3 | shared helpers, core libraries, logging, server backend | helpers.md, build.md, licence.md |"
- missing-devnotes-topics.md:29 — "| build | build.md | decided: replicate MangoWM's build system … Still owed: the dependency set and the wlroots and scenefx version coupling; the exact scenefx extensions needed for user GLSL shaders and 3D transforms, and whether it is a maintained patch or a fork; the solver's arithmetic width … A flake.nix is wanted, with cache.nixos.org set explicitly as the substituter … | 3 | P0 |"
- generaldesign.md:490-491 — "Which wlroots release the build tracks is unresolved; scenefx 0.5 requires wlroots 0.20."
- generaldesign.md:487-489 — "The exact set of scenefx extensions, and whether they are a maintained patch or a fork, is unresolved."
wlroots is required by server.md:12-13 (struct wl_display, struct wl_event_loop) and by helpers.md:26 ("server event-loop/wl_signal conventions"). The version is unresolved, the dependency set is unwritten, and no build file exists. (Per your rule 5, I did not touch /nix or run any build.)
3.2 (a BLOCKER) the log and util contracts that stage 3 delivers are explicitly deferred to stage 3 and are not written
- architecture-audit.md:969 — "| full log and util contracts (helpers.md §7) | roadmap stage 3 |"
- architecture-audit.md:972-975 — "Two of these are recorded as open items in the documents that own them, and that is deliberate: helpers.md §11 flags the binding path and the omni_event payload question"
- helpers.md:471-481 §7 in its entirety — "Ports per the port rule, fresh prefixes. log: mango log.c conventions (WLR levels, file/line prefix on error) plus a component-name tag: [tags] [input] [decorate]. Level set by a block key wm.log.level at boot. util: mango util.c patterns (string_printf, monotonic clock) extended with the container kit components reach for: dynamic array, hash map, linked list, string builder, ring buffer. No component writes its own container."
Eleven lines. No function signature, no level enumeration behind wm.log.level, no sink (stderr / file / ring), no thread-safety statement, no container API. "WLR levels" defers to an unpinned wlroots (generaldesign.md:490). A stage-3 implementer has nothing to write.
3.3 (a BLOCKER) omni_action_schema is defined but never attached to anything
- helpers.md:355-359 — struct omni_action_schema { uint16_t n_args; const uint16_t *arg_tags; const char *result_tag; };
- helpers.md:351-352 — "An action declares its arguments as typed tags, and the declaration is part of the registration rather than documentation"
- vs. helpers.md:186-189 — the descriptor's action type is const char *name; void (*handler)(const struct omni_args *args); — no schema field, no context field
- vs. helpers.md:323 — "omni_action_register(name, handler, context) during activation returns an omni_action_handle" — a 3-argument API that the omni_action struct cannot express
- vs. helpers.md:104-125 struct omni_component — actions/n_actions only
§6.1 is normative for ipc.md §4 exec (ipc.md:658 "The schema lives with the action (helpers.md §6.1), not here"), for ipc.md §3.2, and for ipc.md:295. There is no field anywhere that carries it.
3.4 (a BLOCKER) helpers.md §6.2's client_rule wire form is the pre-windows.md schema, contradicted by four other places
- helpers.md:455-459 — { "type": "client_rule", "value": { "match": { "appid": "org.mozilla.firefox" }, "effect": "join_group", "target": "layout.browser", "scope": "always" } }
- configstorage.md:305-316 — "The record lost five fields it used to have, and each loss is a decision the language made … match_appid, match_title and the TITLE_REGEX flag are gone from this record … target_id and target_gen are gone for the same reason … scope, and with it MAP_ONLY and ALWAYS, is gone"
- ipc.md:370-371 — "Which of the two namespaces a set is under is what its rules do, so there is no effect field to disagree with it, and nothing in a rule matches a window, so there is no match, target or scope."
- ipc.md:295 — the §3 table row: object {"rule","against","axis","edge","region","width","height","by"}
- omni_layout.h:511-520 — "There is no appid, no title and no title-regex flag … There is no target, for the same reason. There is no scope … There is no effect field because the namespace carries it"
- layoutengine.md:1764-1775 — "the matchers, the target and the scope are gone, the namespace carries the effect, and the record is the language's own keys in 24 bytes with no payload"
Five documents agree; helpers.md §6.2 is the lone stale copy. helpers.md is the document stage 3 delivers, and §6.2 is the only place the binding/rule encodings are declared to live (ipc.md:293, helpers.md:399-400).
3.5 (a BLOCKER) server.md and helpers.md declare different struct omni_server
- server.md:11-17 — struct omni_server { struct wl_display *display; struct wl_event_loop *loop; struct omni_store *store; int sock_fd; struct omni_registry *registry; };
- helpers.md:488-494 — struct omni_server { struct wl_display *display; struct wl_event_loop *loop; void *block; int sock_fd; struct omni_registry *registry; };
Field 3 is struct omni_store *store in one and void *block in the other. struct omni_store is named only at server.md:14 and is defined nowhere; void *block loses the type. And:
- server.md:6 — "Anything about the server lifecycle stated in two places is a bug."
- helpers.md:504-507 — "server.md is the authority for all of the above. This section records where the struct is declared … the boot order, readiness mapping, dispatch model, and shutdown sequence are defined there and are not restated"
- but helpers.md:497-502 restates the boot order and the shutdown sequence verbatim.
Two of the three constraints are violated by the third.
3.6 (a BLOCKER) omni_boot has two declared homes
- helpers.md:22 — registry.{c,h}  global registry, register API, **omni_boot sort+activate**
- helpers.md:28 — server.{c,h}   server struct, wl_display lifecycle, register-table, **omni_boot**
- server.md:39-40 — "3. run the register-function table from core/server.c … 4. omni_boot(): create instances in priority order"
- helpers.md:50-51 — "Phase 2, activate: omni_boot() is the only entry point that turns descriptors into live instances"
server.md is the declared authority for server lifecycle; it assigns the register table to core/server.c and omni_boot() to step 4 without saying which file owns the function. helpers.md assigns it to both registry.c and server.c.
3.7 (a BLOCKER) omni_event cannot carry an EVENT-kind journal entry, and that is a recorded open item
- helpers.md:284-286 — "Event struct omni_event carries the journal identity fields: epoch, commit_id, journal_seq, entry_id, entry_generation, kind, time_ns, key or pattern matched, and the typed value as read from the entry."
- The journal slot has 11 fields (configstorelayout.md:426-439): type_tag, flags, body_ref, event_ref are absent from the list, and COMMIT_END carries its group size only in event_ref (configstorelayout.md:455-459 "…event_ref equal to the number of preceding journal entries in the same commit").
- ipc.md:522-523 — the third push line is { "sub": 2, "event": "custom", "category": "ext.foo.signal", "value": "hello", ... } — a category field with no representation in omni_event.
- helpers.md:582-584 — "omni_event field set is fixed by the journal slots (configstorelayout.md §8); richer in-process payloads need a follow-up design if components want them"
So an in-process trigger cannot receive a COMMIT_END (so cannot detect group boundaries, which helpers.md:302-303 requires: "no partial group is dispatched without its COMMIT_END") and cannot receive a custom event.
3.8 (a BLOCKER) helpers.md §11 defers the keybinding path, which layoutengine.md lists as blocking helpers.md, and architecture-audit.md defers to stage 4
Three-way allocation of the same item:
- helpers.md:585-591 — "What is still open is the binding key-to-action binding path for the input subsystem, which consumes the binding type this step defines but is not yet designed. The modmask text grammar ("ctrl+alt") and the keysym name table are still open … so they belong with the input design rather than here."
- layoutengine.md:1733 — "Blocking, meaning this document cannot be finished first: | helpers.md §11 | the key-to-action binding path, which is how a layout is chosen from a keypress, recorded there as not yet designed |"
- architecture-audit.md:968 — "| the keybinding path, modmask grammar, keysym names (helpers.md §11) | roadmap stage 4 |"
- layoutengine.md:1725 — "| input.md | §7.6's snap needs a pointer position and a keybind with a direction argument … |" — input.md is blocked by layoutengine.md, which is stage 6.
So stage 3 is blocked by an item deferred to stage 4, and stage 4 is blocked by stage 6. layoutengine.md:1733 names helpers.md §11 as a blocker; helpers.md:588 says it "is not yet designed". Neither can proceed.
3.9 (b DOCUMENTED DEFERRAL, acceptable) the rest of helpers.md §11
- helpers.md:593-595 — "Component dependency edges beyond plain priority (a hard 'requires X') do not exist at v1, and are not planned; priority ordering is the mechanism permanently unless a real dependency failure appears."
- helpers.md:596-599 — "The owner field on an instance is diagnostic only. Nothing enforces it and no access check consults it."
Both are stated as decisions, not gaps. helpers.md §8.1 (no plugin ABI) is likewise an explicit rejection, not a deferral.
3.10 (b DOCUMENTED DEFERRAL) licence.md
- missing-devnotes-topics.md:31 — "decided: GPL-3.0 … Still owed: the licence file itself, and a provenance record for anything actually copied, which is a chore at the moment of the first copy rather than a design question"
- layoutengine.md:1735 — "licence.md | resolved. The project is GPL-3.0 … The provenance record is what remains, and it is a small chore at the moment something is copied rather than a design question"
- architecture-audit.md:31 — licence: P3, the lowest band
Explicitly acceptable to defer. The licence decision is closed; only the file is missing.
STAGE 4 — basic input
4.1 (a BLOCKER, confirmed) input.md DOES NOT EXIST
ls /home/den/repos/omniWM/devnotes/ returns 16 files. No input.md. The name appears in exactly six places, all of them pointing at a file that is absent:
- generaldesign.md:469 — "| 4 | basic input | input.md |"  ← the stage's only named document
- generaldesign.md:475 — "| 10 | advanced input | input.md |"
- missing-devnotes-topics.md:25 — "| input | input.md | one catalog key per binding; matching across keyboard, mouse buttons, wheel, touchpad gestures, tablet and stylus; gesture recognition on top of the wlroots signals; invoking the action registry; focus-follows-mouse and click-to-focus; tablet pressure, tilt, absolute pointing and annotation mode | 4, 10 | P0 |"
- windows.md:901 — "how a gesture reaches an action (input.md, helpers.md)"
- tags.md:372 — "The surface syntax for setting, unsetting and exclusively setting a tag, which belongs to input.md and helpers.md §11."
- layoutengine.md:1725 — "| input.md | §7.6's snap needs a pointer position and a keybind with a direction argument converted into a chosen area, and helpers.md §11 records the key-to-action path as undesigned |"
architecture-audit.md:965 — "| §4 constraint layouts, tags, scene, renderer, input, protocol surface | roadmap stages 4 to 14; devnotes/layoutsystem.md and devnotes/looks.md remain empty by decision |" — input was scoped out of the audit that closed stages 1-2.
There is also no devnote at all for the device layer: server.md:155-157 — "Display and output lifecycle, input devices, and the Wayland protocol set are not designed."
4.2 What stage 4 therefore lacks, itemised
missing-devnotes-topics.md:25 enumerates what input.md must answer. Every one is absent:
required by missing-devnotes-topics.md:25
one catalog key per binding
matching across keyboard, mouse buttons, wheel, touchpad gestures, tablet, stylus
gesture recognition on top of the wlroots signals
invoking the action registry
focus-follows-mouse and click-to-focus
tablet pressure, tilt, absolute pointing, annotation mode
The only input design that exists is six prose bullets, generaldesign.md:389-404, of which the operative sentence is generaldesign.md:403-404 — "wlroots supplies the device plumbing; the matching from a device event to a configured action, including all gesture recognition, is omniWM's work." That is the whole of it.
4.3 (a BLOCKER) stage 4 is blocked by stage 6
- layoutengine.md:1717 — "Blocked, meaning the other document cannot be finished first:" … :1725 — "| input.md | §7.6's snap needs a pointer position and a keybind with a direction argument converted into a chosen area, and helpers.md §11 records the key-to-action path as undesigned |"
input.md (stage 4) cannot be finished before layoutengine.md (stage 6). This is a dependency inversion in the roadmap, and it is the reason layoutengine.md:1725 is in the "Blocked" table rather than the open-items list.
4.4 (a BLOCKER) the modmask grammar and the keysym name table — required to write a binding — are deferred to stage 4 and are unwritten in two places
- helpers.md:589-591 — "The modmask text grammar ("ctrl+alt") and the keysym name table are still open, and they are what a keybinding author actually types, so they belong with the input design rather than here."
- ipc.md:952-953 — "The modmask text grammar ("ctrl+alt") and keysym string names need a canonical table; deferred until bindings exist (layout/bindings work)."
- ipc.md:278 — the wire form is modmask | string "ctrl+alt" or array | string "ctrl+alt" — the grammar is asserted, never specified.
- ipc.md:276 — keysym | string or number | string (XKB name) — the name table is asserted, never specified.
- configstorage.md:164 — "| 0x14 | modmask | u32 bitmask (ctrl/alt/shift/super/etc) |" — the bit positions are never given either.
ipc.md:952-953 says "deferred until bindings exist" and architecture-audit.md:968 says "roadmap stage 4", but stage 4 is the stage that needs them. Circular: the encoding is deferred until the feature exists, and the feature cannot be built without the encoding.
4.5 (a BLOCKER) struct omni_binding's device coverage is not designed
configstorage.md:222-231 gives a binding a modmask, a keysym, a keycode, an action_ref, args_ref, flags with bit 0 USE_KEYSYM (omni_layout.h:592). There is no field for a mouse button, a wheel direction, a touchpad gesture, or a tablet axis. generaldesign.md:395-396 requires "Bindings cover the keyboard, mouse buttons, the wheel, touchpad gestures, and tablet and stylus input." missing-devnotes-topics.md:25 requires the same. The 24-byte record in configstorage.md:222-231 and omni_layout.h:583-590 can express keyboard only. Whether stage 4 extends the tag, adds tags, or routes non-keyboard input through actions is unstated in every document — and configstorage.md:141 says "The list is intentionally broad for now; redundant options can be culled later," which is the opposite of a decision.
4.6 (b DOCUMENTED DEFERRAL, acceptable) advanced input is stage 10, not 4
generaldesign.md:475 and missing-devnotes-topics.md:25 both assign input.md to stages "4, 10". The stage-4/stage-10 split of input.md's own content is not written down — nothing says which of the seven required topics lands at 4 and which at 10. architecture-audit.md:970 — "| a scene node and animatable property model | roadmap stages 5 and 9 |" shows the same 2-stage pattern elsewhere with no split table either. This is a scoping ambiguity, not a contradiction.
4.7 Note on include/input/
Seven headers exist (input.h, binds.h, devices.h, gestures.h, keyboard.h, mouse.h, tablet.h, trackpad.h) and seven .c files, all 0 bytes. Their existence implies a file decomposition that no document sanctions — filestructure.md:88-90 says "Their designs are not written, so no directory here has a documented contract yet, and this document does not invent one." Note that filestructure.md:88-90 is itself stale for windows, tags and the layout documents, which now exist.
Cross-cutting: dangling references, collected
reference
layoutengine.md:383 "one constant in omni_layout.h §8"
layoutengine.md:1948 "done, now OMNI_CONSTRAINT_PRIORITY_DOMINANT"
layoutengine.md:1948 "rename OMNI_CONSTRAINT_PRIORITY_REQUIRED"
layoutengine.md:1947 "devnotes/layoutsystem.md | new | … | open"
missing-devnotes-topics.md:12 "The two empty placeholders layoutsystem.md and looks.md"
tags.md:403 "configstorage.md §8 | WINDOW_DEPENDENT scope, and the save and reset classification"
ipc.md:6, :484 "§12 of configstorelayout.md" for the commit protocol
ipc.md:461 "configstorelayout.md §12 guard bundle"
omni_layout.h:57 "devnotes/configstorelayout.md sections 2-10 (byte layout, commit protocol)"
ipc.md:29 "single definition point, include/shared/omni_layout.h" for 6 OMNI_SOCK_*
ipc.md:386-394 for 8 OMNI_VALUE_MAX_*
configstorage.md:718, ipc.md:719, tomlparser.md:228 for OMNI_CONFIG_MAX_OPS
layoutengine.md:396 wm.cycle_layout taking "master-stack" (hyphen)
layoutengine.md:1940 "ipc.md | §3 type encodings | … | done" and layoutengine.md:1941-1942 "tomlparser.md §2 … done"
configstorelayout.md:22 "two tags (OMNI_TAG_CONSTRAINT, OMNI_TAG_CLIENT_RULE …, a framed OMNI_TAG_BINDING, and a seventh fixed section"
Cross-cutting: ipc.md command and encoding completeness
Commands. All nine in ipc.md:178 are specified: get (:449), set (:463), watch (:493), unwatch (:540-543, no ### heading but fully specified), delete (:739), exec (:646), save (:670), reload (:699), reset (:758). Pre-ready behaviour is tabulated per command at :882-890. reset's two modes have a scope matrix at :803-812. Commands are complete.
Encodings. ipc.md:260-304 covers 51 of the 52 tags in configstorage.md:143-198. Missing: map (0x34). Per-tag width mismatch: configstorage.md:156 char is "4 bytes (UTF-32 code point)" and ipc.md:273 is "string (1 char)" — consistent. Composite discriminators are settled at ipc.md:332-371. Wide integers at :306-330. Limits at :373-406. Encoding is one tag short, and that one tag is the window-rule block that ipc.md:932-942 itself makes mandatory.
One notation inconsistency: ipc.md:185-186 writes "epoch": <u64>, "commit_id": <u64> bare, while ipc.md:315-317 says the {"$u64":"…"} wrapper applies "to epoch, commit_id, and journal_seq in every response and event."
Cross-cutting: the map tag and windows.md — a stage-5 document stage 1 depends on
Worth flagging because it inverts the roadmap. ipc.md:932-942 — "The readable client-field set and the window-rule filter vocabulary are the same set, and this is a consequence rather than a new demand. windows.md §9.3 splits a client's fields into observable and settable … The check is mechanical: a field that appears in an if block must appear in what a client read returns."
windows.md is stage 5/8 (generaldesign.md:470, missing-devnotes-topics.md:20). missing-devnotes-topics.md:20 rates it "written, prototype" — the only compositor-subsystem document that exists. So stage 1's socket surface is contractually gated on a stage-5 prototype, and the map wire form that would carry the rules is missing from ipc.md entirely. windows.md:486-487 — "Rules are block values, held as a referenced group at omniwm.window_rules.<name>, readable and writable through the same route as everything else."
Summary of the (a) genuine blockers
#
1
2
3
4
5
6
7
8
9
10
11
12
13
14
15
16
17
18
19
20
21
22
23
24
25
26
27
(b) Documented deferrals, explicitly acceptable
configstorage.md:745-749 (multi-painter, CAS, mixed-endianness, extension semantic validation, compile-time ABI tests) · configstorelayout.md:849-862 (v1 capacity choices, with rationales) · tomlparser.md:265-269, 275-278 (TOML grammar upstream, datetime arithmetic) · ipc.md:948-977 §8 except the binding/rule bullet at :961-965, which is stale (those are defined in helpers.md §6.1-6.2) · helpers.md:593-599 (dependency edges, owner field) · licence.md's missing file and provenance record (missing-devnotes-topics.md:31, layoutengine.md:1735) · architecture-audit.md:963-970 deferral table, whose two stage-3/stage-4 rows (:968-969) are the ones blocking stages 3 and 4.
Bottom line
Stage 1 is one careful editing pass from implementable, not blocked by a design question: items 1-3, 10 are stale text, item 5 needs one decision, items 4/6/7/8 are omissions, and item 9 (testing.md) is a short document. The write+read test itself needs only §3, §5, §6, §7, §8, §9, §10, §13 of configstorelayout.md — all of which are byte-exact and internally consistent once the §3 prose is corrected.
Stage 2 is blocked: three phantom tags, a three-way disagreement on the program's key shape, and a field-name conflict. tomlparser.md cannot be written against the current store.
Stage 3 is blocked on two missing documents and on helpers.md §6.1/§6.2 being unwired to any struct and partly stale.
Stage 4 is entirely blocked: input.md does not exist, it is downstream of stage 6, the keybinding encoding it needs is deferred to itself, and the store's binding record cannot express a non-keyboard binding. 
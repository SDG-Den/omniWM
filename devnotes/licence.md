# Licence (devnotes/licence.md)

The licence is GPL-3.0, and it is decided rather than open. `architecture-audit.md`
§5 fixes it from Mango's own `LICENSE`, and the project is a Mango fork under
`generaldesign.md` §14's port rule, so GPL-3.0 is the licence the port is
compatible with. DuckWM's and MangoWM's licences are therefore not an obstacle to
reuse, and neither are most other window managers', because GPL-3.0 is what the
copy would have to be compatible with and it is the compatible direction.

This document was phase 02's B7. What it owns is small and concrete: the licence
file itself, and a provenance record for anything actually copied.

## 1. The licence file

`LICENSE` at the repository root holds the GPL-3.0 text, verbatim from the Free
Software Foundation. It is the file the licence requires to be distributed with
the work, and it is the file a consumer looks for.

## 2. The provenance record

`PROVENANCE.md` at the repository root records every piece of third-party code or
definition that is copied or ported in. It is the record the GPL's preservation
obligations need traceable to a source, and it exists now rather than at the
first copy because the first copy is close: `input.md` §3 is about to be the
largest copy in the project, and a provenance record that is written after the
copy is a record that has to be reconstructed.

The rule `PROVENANCE.md` states is the one this document commits to: an entry is
added at the moment of the copy, a port counts as a copy, and the applicable
copyright notice is preserved at the point of the copy. The four entries it
already carries are the three vendored wlroots protocol definitions and the
adopted `protocols/meson.build`; each protocol XML carries its own permissive
notice in the file (X11-style for two, MIT for one), which is what its licence
requires.

## 3. What the record is not

It is not a list of every dependency. A library linked from nixpkgs is not copied
into this repository, so it is not a provenance row; it is a build input, and its
licence is nixpkgs' concern. A row means bytes in this tree that came from
somewhere else, and nothing else.

## 4. Open items

None. The licence is decided, the file exists, and the record exists with its
rule and its first entries. The document has no deferred questions, which is why
`missing-devnotes-topics.md` marked it P3.

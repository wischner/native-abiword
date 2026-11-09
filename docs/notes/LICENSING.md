# Licensing

Fyodor is licensed under the GNU General Public License, version 2 or (at your
option) any later version, the same terms as AbiWord. `LICENSE` is the GPL v2
text, copied from AbiWord's `COPYING`.

AbiWord's source is GPL-2.0-or-later and the files here port it, so they are
derived works. That rules out the MIT header in the copied coding standard:
source headers read `GPL 2.0 or later (see: LICENSE)` instead. This is the one
deliberate difference from `docs/standards/CPP-CODING-STYLE.md`, which is kept
as copied.

Files derived from a specific AbiWord file name it and keep its copyright line.
The GPL's notice requirement needs that, so keep those lines when moving code.
Files written from scratch carry only the project line.

Native is MIT, which is compatible, and is linked statically. "AbiWord" is
AbiSource's name, so the product is called Fyodor and only the file format keeps
the AbiWord name.

# Port status

What has moved from AbiWord `6940426ea` and what has not.

## Ported

| AbiWord | fyodor | Notes |
| --- | --- | --- |
| `ut_units`, `ut_color` | `lib/util` | Named colors reduced to common ones |
| `pf_Fragments`, `pf_Frag*` | `lib/doc/fragments`, `frag` | No FmtMark fragment |
| `pp_AttrProp`, `pp_TableAttrProp` | `lib/doc/attr_prop` | |
| `pp_Property` table | `lib/doc/properties` | All 185 properties, including ones nothing reads yet |
| `px_ChangeRecord`, `px_ChangeHistory` | `lib/doc/change_record` | No remote collaboration handling |
| `pt_PieceTable`, `pt_PT_*` | `lib/doc/piece_table*` | |
| `pd_Style`, `PP_evalProperty` | `lib/doc/document` | First seven built-in styles only |
| `fp_Run`, `fp_TextRun` | `lib/fmt/run` | Fields show placeholders, see below |
| `fb_LineBreaker`, `fb_Alignment`, `fp_Line` | `lib/fmt/block_layout` | |
| `fb_ColumnBreaker`, `fp_Page`, `fp_Column` | `lib/fmt/column_breaker`, `doc_layout` | |
| `ie_imp_AbiWord_1`, `ie_exp_AbiWord_1` | `lib/impexp/abw_*` | |
| `ie_imp_Text`, `ie_exp_Text` | `lib/impexp/text_format` | UTF-8 only |
| `ie_exp_HTML` | `lib/impexp/html_exporter` | Subset; images are not exported |

## Not ported

- Tables, frames, footnotes, endnotes, annotations, tables of contents. The
  importer flattens them to paragraphs and warns; **saving such a file loses the
  structure**.
- Headers and footers: kept in the file, not displayed.
- Lists: `list_label` fields draw a bullet; numbering and the `<lists>` section
  are not implemented, and that section is lost on save.
- Page number and page count fields draw `#`.
- Right-to-left text, BiDi and complex shaping.
- Revisions, `<history>`, RDF, bookmarks and hyperlinks as active objects.
- Ruler clicks that add a tab stop; existing stops can be dragged.
- Spell checking, grammar, mail merge, collaboration, scripting and plugins.
- Other file formats (RTF, DOCX, ODT, PDF) and printing.
- The list styles and numbered heading styles.

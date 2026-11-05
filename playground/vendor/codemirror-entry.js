// Everything the playground uses from CodeMirror 6, bundled into one file
// (codemirror.js) so every part shares a single copy of @codemirror/state.
export { EditorState, Compartment, StateField, StateEffect, RangeSetBuilder } from "@codemirror/state";
export {
  EditorView, keymap, lineNumbers, highlightActiveLine, highlightActiveLineGutter,
  drawSelection, Decoration, placeholder,
} from "@codemirror/view";
export {
  StreamLanguage, syntaxHighlighting, HighlightStyle, bracketMatching, indentOnInput, indentUnit,
} from "@codemirror/language";
export { defaultKeymap, history, historyKeymap, indentWithTab } from "@codemirror/commands";
export { setDiagnostics, lintGutter } from "@codemirror/lint";
export { closeBrackets, closeBracketsKeymap } from "@codemirror/autocomplete";
export { searchKeymap, highlightSelectionMatches } from "@codemirror/search";
export { tags } from "@lezer/highlight";

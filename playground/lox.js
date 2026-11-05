// Lox syntax highlighting for CodeMirror: a small tokenizer (StreamLanguage)
// and a highlight style whose colours come from CSS variables, so the same
// style works in the light and the dark theme (style.css sets the variables).

import { StreamLanguage, HighlightStyle, syntaxHighlighting, tags } from "./vendor/codemirror.js";

const KEYWORDS = new Set([
  "and", "class", "else", "for", "fun", "if", "or", "print", "return", "var", "while",
]);
const ATOMS = new Set(["true", "false", "nil"]);

const loxParser = {
  name: "lox",
  // inString: inside a string that started on an earlier line (Lox strings
  // may span lines). expect: what the previous word makes the next name.
  startState: () => ({ inString: false, expect: null }),
  copyState: (s) => ({ ...s }),

  token(stream, state) {
    if (state.inString) {
      while (!stream.eol()) {
        if (stream.next() === '"') {
          state.inString = false;
          return "string";
        }
      }
      return "string";
    }
    if (stream.eatSpace()) return null;

    if (stream.match("//")) {
      stream.skipToEnd();
      return "comment";
    }
    if (stream.peek() === '"') {
      stream.next();
      while (!stream.eol()) {
        if (stream.next() === '"') return "string";
      }
      state.inString = true;   // continues on the next line
      return "string";
    }
    if (stream.match(/^\d+(\.\d+)?/)) {
      state.expect = null;
      return "number";
    }
    const word = stream.match(/^[A-Za-z_][A-Za-z0-9_]*/);
    if (word) {
      const text = word[0];
      const expect = state.expect;
      state.expect = null;
      if (KEYWORDS.has(text)) {
        if (text === "fun") state.expect = "function";
        if (text === "class") state.expect = "class";
        return "keyword";
      }
      if (ATOMS.has(text)) return "atom";
      if (text === "this" || text === "super") return "self";
      if (expect === "function") return "variableName.definition.function";
      if (expect === "class") {
        state.expect = "superclass";   // `class A < B`: B is a class too
        return "className.definition";
      }
      if (expect === "superclass" || expect === "afterLess") return "className";
      if (expect === "property") return "propertyName";
      return "variableName";
    }

    const ch = stream.next();
    if (ch === ".") {
      state.expect = "property";
      return "punctuation";
    }
    if (ch === "<" && state.expect === "superclass") {
      state.expect = "afterLess";
      return "operator";
    }
    state.expect = null;
    if ("+-*/=!<>".includes(ch)) {
      stream.eat("=");
      return "operator";
    }
    return "punctuation";
  },

  languageData: {
    commentTokens: { line: "//" },
    closeBrackets: { brackets: ["(", "{", '"'] },
  },
};

export const lox = StreamLanguage.define(loxParser);

export const loxHighlighting = syntaxHighlighting(
  HighlightStyle.define([
    { tag: tags.keyword, color: "var(--tok-keyword)", fontWeight: "600" },
    { tag: [tags.atom, tags.bool, tags.null], color: "var(--tok-atom)" },
    { tag: tags.number, color: "var(--tok-number)" },
    { tag: tags.string, color: "var(--tok-string)" },
    { tag: tags.comment, color: "var(--tok-comment)", fontStyle: "italic" },
    { tag: tags.self, color: "var(--tok-self)", fontStyle: "italic" },
    { tag: [tags.function(tags.definition(tags.variableName)), tags.definition(tags.className)],
      color: "var(--tok-def)", fontWeight: "600" },
    { tag: tags.className, color: "var(--tok-def)" },
    { tag: tags.propertyName, color: "var(--tok-prop)" },
    { tag: tags.operator, color: "var(--tok-operator)" },
  ]),
);

// Unit tests for the scanner.
//
// The first six tests port the official test/scanning/*.lox files: same input,
// same expected tokens. Upstream those files expect jlox's token-dump format
// (e.g. "NUMBER 123 123.0"), which clox never prints, so the book's own test
// runner skips them for every clox chapter. Checking the tokens directly here
// keeps their coverage.

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "scanner.h"

static int failures = 0;
static int checks = 0;

typedef struct {
  TokenType type;
  const char* lexeme;  // exact source text of the token
  int line;            // 0 = don't check
} Expected;

#define END {TOKEN_EOF, "", 0}

// Scans `source` and checks it produces exactly `expected` (ending with EOF).
static void expectTokens(const char* name, const char* source,
                         const Expected* expected) {
  initScanner(source);
  for (int i = 0;; i++) {
    Token token = scanToken();
    const Expected* want = &expected[i];
    checks++;

    int lexemeLength = (int)strlen(want->lexeme);
    bool ok = token.type == want->type &&
              token.length == lexemeLength &&
              memcmp(token.start, want->lexeme, (size_t)lexemeLength) == 0 &&
              (want->line == 0 || token.line == want->line);
    if (!ok) {
      failures++;
      fprintf(stderr, "%s: token %d: expected type %d '%s' (line %d), "
              "got type %d '%.*s' (line %d)\n",
              name, i, want->type, want->lexeme, want->line,
              token.type, token.length, token.start, token.line);
      return;
    }
    if (want->type == TOKEN_EOF) return;
  }
}

static void testIdentifiers(void) {
  Expected want[] = {
    {TOKEN_IDENTIFIER, "andy", 1},
    {TOKEN_IDENTIFIER, "formless", 1},
    {TOKEN_IDENTIFIER, "fo", 1},
    {TOKEN_IDENTIFIER, "_", 1},
    {TOKEN_IDENTIFIER, "_123", 1},
    {TOKEN_IDENTIFIER, "_abc", 1},
    {TOKEN_IDENTIFIER, "ab123", 1},
    {TOKEN_IDENTIFIER,
     "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890_", 2},
    END,
  };
  expectTokens("identifiers",
      "andy formless fo _ _123 _abc ab123\n"
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890_\n",
      want);
}

static void testKeywords(void) {
  Expected want[] = {
    {TOKEN_AND, "and", 1},     {TOKEN_CLASS, "class", 1},
    {TOKEN_ELSE, "else", 1},   {TOKEN_FALSE, "false", 1},
    {TOKEN_FOR, "for", 1},     {TOKEN_FUN, "fun", 1},
    {TOKEN_IF, "if", 1},       {TOKEN_NIL, "nil", 1},
    {TOKEN_OR, "or", 1},       {TOKEN_RETURN, "return", 1},
    {TOKEN_SUPER, "super", 1}, {TOKEN_THIS, "this", 1},
    {TOKEN_TRUE, "true", 1},   {TOKEN_VAR, "var", 1},
    {TOKEN_WHILE, "while", 1},
    END,
  };
  expectTokens("keywords",
      "and class else false for fun if nil or return super this true var while\n",
      want);
}

// Not in the official file: words that start like a keyword but aren't one.
// These exercise each branch of identifierType()'s trie.
static void testKeywordPrefixes(void) {
  Expected want[] = {
    {TOKEN_IDENTIFIER, "an", 0},     {TOKEN_IDENTIFIER, "andy", 0},
    {TOKEN_IDENTIFIER, "f", 0},      {TOKEN_IDENTIFIER, "fa", 0},
    {TOKEN_IDENTIFIER, "fork", 0},   {TOKEN_IDENTIFIER, "fund", 0},
    {TOKEN_IDENTIFIER, "t", 0},      {TOKEN_IDENTIFIER, "thistle", 0},
    {TOKEN_IDENTIFIER, "truely", 0}, {TOKEN_IDENTIFIER, "printer", 0},
    {TOKEN_IDENTIFIER, "Print", 0},  {TOKEN_IDENTIFIER, "variable", 0},
    {TOKEN_PRINT, "print", 0},
    END,
  };
  expectTokens("keyword prefixes",
      "an andy f fa fork fund t thistle truely printer Print variable print",
      want);
}

static void testNumbers(void) {
  Expected want[] = {
    {TOKEN_NUMBER, "123", 1},
    {TOKEN_NUMBER, "123.456", 2},
    {TOKEN_DOT, ".", 3},         // a leading dot is not part of a number
    {TOKEN_NUMBER, "456", 3},
    {TOKEN_NUMBER, "123", 4},    // nor is a trailing one
    {TOKEN_DOT, ".", 4},
    END,
  };
  expectTokens("numbers", "123\n123.456\n.456\n123.\n", want);
}

static void testPunctuators(void) {
  Expected want[] = {
    {TOKEN_LEFT_PAREN, "(", 1},     {TOKEN_RIGHT_PAREN, ")", 1},
    {TOKEN_LEFT_BRACE, "{", 1},     {TOKEN_RIGHT_BRACE, "}", 1},
    {TOKEN_SEMICOLON, ";", 1},      {TOKEN_COMMA, ",", 1},
    {TOKEN_PLUS, "+", 1},           {TOKEN_MINUS, "-", 1},
    {TOKEN_STAR, "*", 1},           {TOKEN_BANG_EQUAL, "!=", 1},
    {TOKEN_EQUAL_EQUAL, "==", 1},   {TOKEN_LESS_EQUAL, "<=", 1},
    {TOKEN_GREATER_EQUAL, ">=", 1}, {TOKEN_BANG_EQUAL, "!=", 1},
    {TOKEN_LESS, "<", 1},           {TOKEN_GREATER, ">", 1},
    {TOKEN_SLASH, "/", 1},          {TOKEN_DOT, ".", 1},
    END,
  };
  expectTokens("punctuators", "(){};,+-*!===<=>=!=<>/.\n", want);
}

// Not in the official file: the single-character forms of ! = < >.
static void testSingleCharOperators(void) {
  Expected want[] = {
    {TOKEN_BANG, "!", 1}, {TOKEN_EQUAL, "=", 1},
    {TOKEN_LESS, "<", 1}, {TOKEN_GREATER, ">", 1},
    END,
  };
  expectTokens("single-char operators", "! = < >", want);
}

static void testStrings(void) {
  // The lexeme includes the quotes; the compiler strips them (chapter 19).
  Expected want[] = {
    {TOKEN_STRING, "\"\"", 1},
    {TOKEN_STRING, "\"string\"", 2},
    END,
  };
  expectTokens("strings", "\"\"\n\"string\"\n", want);
}

static void testWhitespace(void) {
  Expected want[] = {
    {TOKEN_IDENTIFIER, "space", 1},
    {TOKEN_IDENTIFIER, "tabs", 1},
    {TOKEN_IDENTIFIER, "newlines", 1},
    {TOKEN_IDENTIFIER, "end", 6},
    {TOKEN_EOF, "", 7},
  };
  expectTokens("whitespace",
      "space    tabs\t\t\t\tnewlines\n\n\n\n\nend\n", want);
}

// --- beyond the official scanning tests ---------------------------------------

static void testExpression(void) {
  Expected want[] = {
    {TOKEN_NUMBER, "1", 1}, {TOKEN_PLUS, "+", 1}, {TOKEN_NUMBER, "2", 1},
    {TOKEN_STAR, "*", 1},   {TOKEN_NUMBER, "3", 1},
    END,
  };
  expectTokens("1 + 2 * 3", "1 + 2 * 3", want);
}

static void testComments(void) {
  Expected want[] = {
    {TOKEN_NUMBER, "1", 1},
    {TOKEN_SLASH, "/", 2},   // a single slash is division, not a comment
    {TOKEN_NUMBER, "2", 2},
    {TOKEN_EOF, "", 3},      // comment at end of file without a newline
    END,
  };
  expectTokens("comments", "1 // one\n/ 2\n// trailing comment", want);
}

static void testMultilineStringCountsLines(void) {
  Expected want[] = {
    {TOKEN_STRING, "\"a\nb\nc\"", 3},  // line is where the token *ends*
    {TOKEN_NUMBER, "1", 4},
    END,
  };
  expectTokens("multi-line string", "\"a\nb\nc\"\n1", want);
}

static void testErrors(void) {
  // For TOKEN_ERROR the "lexeme" is the error message.
  Expected unterminated[] = {
    {TOKEN_ERROR, "Unterminated string.", 2},
    END,
  };
  expectTokens("unterminated string", "\"never\nclosed", unterminated);

  Expected unexpected[] = {
    {TOKEN_NUMBER, "1", 1},
    {TOKEN_ERROR, "Unexpected character.", 1},
    {TOKEN_ERROR, "Unexpected character.", 1},
    {TOKEN_NUMBER, "2", 1},
    END,
  };
  expectTokens("unexpected characters", "1 @# 2", unexpected);
}

static void testEmptyAndEofRepeats(void) {
  // Asking for more tokens after EOF must keep returning EOF, never read
  // past the '\0' terminator. (ASan would catch an over-read here.)
  initScanner("");
  int bad = 0;
  for (int i = 0; i < 3; i++) {
    if (scanToken().type != TOKEN_EOF) bad++;
  }
  checks++;
  if (bad) {
    failures++;
    fprintf(stderr, "empty source: expected repeated EOF\n");
  }
}

int main(void) {
  testIdentifiers();
  testKeywords();
  testKeywordPrefixes();
  testNumbers();
  testPunctuators();
  testSingleCharOperators();
  testStrings();
  testWhitespace();
  testExpression();
  testComments();
  testMultilineStringCountsLines();
  testErrors();
  testEmptyAndEofRepeats();

  if (failures > 0) {
    printf("%d of %d checks failed\n", failures, checks);
    return 1;
  }
  printf("PASS (%d checks)\n", checks);
  return 0;
}

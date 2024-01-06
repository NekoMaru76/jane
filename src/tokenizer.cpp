#include "include/tokenizer.hpp"
#include "include/list.hpp"
#include "include/util.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define WHITESPACE ' ' : case '\n'

#define DIGIT                                                                  \
  '0' : case '1':                                                              \
  case '2':                                                                    \
  case '3':                                                                    \
  case '4':                                                                    \
  case '5':                                                                    \
  case '6':                                                                    \
  case '7':                                                                    \
  case '8':                                                                    \
  case '9'

#define ALPHA                                                                  \
  'a' : case 'b':                                                              \
  case 'c':                                                                    \
  case 'd':                                                                    \
  case 'e':                                                                    \
  case 'f':                                                                    \
  case 'g':                                                                    \
  case 'h':                                                                    \
  case 'i':                                                                    \
  case 'j':                                                                    \
  case 'k':                                                                    \
  case 'l':                                                                    \
  case 'm':                                                                    \
  case 'n':                                                                    \
  case 'o':                                                                    \
  case 'p':                                                                    \
  case 'q':                                                                    \
  case 'r':                                                                    \
  case 's':                                                                    \
  case 't':                                                                    \
  case 'u':                                                                    \
  case 'v':                                                                    \
  case 'w':                                                                    \
  case 'x':                                                                    \
  case 'y':                                                                    \
  case 'z':                                                                    \
  case 'A':                                                                    \
  case 'B':                                                                    \
  case 'C':                                                                    \
  case 'D':                                                                    \
  case 'E':                                                                    \
  case 'F':                                                                    \
  case 'G':                                                                    \
  case 'H':                                                                    \
  case 'I':                                                                    \
  case 'J':                                                                    \
  case 'K':                                                                    \
  case 'L':                                                                    \
  case 'M':                                                                    \
  case 'N':                                                                    \
  case 'O':                                                                    \
  case 'P':                                                                    \
  case 'Q':                                                                    \
  case 'R':                                                                    \
  case 'S':                                                                    \
  case 'T':                                                                    \
  case 'U':                                                                    \
  case 'V':                                                                    \
  case 'W':                                                                    \
  case 'X':                                                                    \
  case 'Y':                                                                    \
  case 'Z'

#define SYMBOL_CHAR                                                            \
  ALPHA:                                                                       \
  case DIGIT:                                                                  \
  case '_'

enum TokenizeState {
  TokenizeStateStart,
  TokenizeStateSymbol,
  TokenizeStateNumber,
  TokenizeStateString,
  TokenizeStateSawDash,
  TokenizeStateSawSlash,
  TokenizeStateLineComment,
  TokenizeStateMultiLineComment,
  TokenizeStateMultiLineCommentSlash,
  TokenizeStateMultiLineCommentStar,
  TokenizeStatePipe,
  TokenizeStateAmpersand,
  TokenizeStateEq,
  TokenizeStateBang,
  TokenizeStateLessThan,
  TokenizeStateGreaterThan,
};

struct Tokenize {
  Buf *buf;
  int pos;
  TokenizeState state;
  JaneList<Token> *tokens;
  int line;
  int column;
  Token *cur_tok;
  int multi_line_comment_count;
};

__attribute__((format(printf, 2, 3))) static void
tokenize_error(Tokenize *t, const char *format, ...) {
  int line;
  int column;
  if (t->cur_tok) {
    line = t->cur_tok->start_line + 1;
    column = t->cur_tok->start_column + 1;
  } else {
    line = t->line + 1;
    column = t->column + 1;
  }
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "error: Line %d, column %d: ", line, column);
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  exit(EXIT_FAILURE);
}

static void begin_token(Tokenize *t, TokenId id) {
  assert(!t->cur_tok);
  t->tokens->add_one();
  Token *token = &t->tokens->last();
  token->start_line = t->line;
  token->start_column = t->column;
  token->id = id;
  token->start_position = t->pos;
  t->cur_tok = token;
}

static void cancel_token(Tokenize *t) {
  t->tokens->pop();
  t->cur_tok = nullptr;
}

static void end_token(Tokenize *t) {
  assert(t->cur_tok);
  t->cur_tok->end_position = t->pos + 1;

  char *token_mem = buf_ptr(t->buf) + t->cur_tok->start_position;
  int token_len = t->cur_tok->end_position - t->cur_tok->start_position;

  if (mem_eql_str(token_mem, token_len, "fun")) {
    t->cur_tok->id = TokenIdKeywordFn;
  } else if (mem_eql_str(token_mem, token_len, "return")) {
    t->cur_tok->id = TokenIdKeywordReturn;
  } else if (mem_eql_str(token_mem, token_len, "mut")) {
    t->cur_tok->id = TokenIdKeywordMut;
  } else if (mem_eql_str(token_mem, token_len, "const")) {
    t->cur_tok->id = TokenIdKeywordConst;
  } else if (mem_eql_str(token_mem, token_len, "extern")) {
    t->cur_tok->id = TokenIdKeywordExtern;
  } else if (mem_eql_str(token_mem, token_len, "unreachable")) {
    t->cur_tok->id = TokenIdKeywordUnreachable;
  } else if (mem_eql_str(token_mem, token_len, "pub")) {
    t->cur_tok->id = TokenIdKeywordPub;
  } else if (mem_eql_str(token_mem, token_len, "export")) {
    t->cur_tok->id = TokenIdKeywordExport;
  } else if (mem_eql_str(token_mem, token_len, "as")) {
    t->cur_tok->id = TokenIdKeywordAs;
  } else if (mem_eql_str(token_mem, token_len, "use")) {
    t->cur_tok->id = TokenIdKeywordUse;
  }

  t->cur_tok = nullptr;
}

JaneList<Token> *tokenize(Buf *buf, Buf *cur_dir_path) {
  Tokenize t = {0};
  t.tokens = allocate<JaneList<Token>>(1);
  t.buf = buf;
  for (t.pos = 0; t.pos < buf_len(t.buf); t.pos += 1) {
    uint8_t c = buf_ptr(t.buf)[t.pos];
    switch (t.state) {
    case TokenizeStateStart:
      switch (c) {
      case WHITESPACE:
        break;
      case ALPHA:
      case '_':
        t.state = TokenizeStateSymbol;
        begin_token(&t, TokenIdSymbol);
        break;
      case DIGIT:
        t.state = TokenizeStateNumber;
        begin_token(&t, TokenIdNumberLiteral);
        break;
      case '"':
        begin_token(&t, TokenIdStringLiteral);
        t.state = TokenizeStateString;
        break;
      case '(':
        begin_token(&t, TokenIdLParen);
        end_token(&t);
        break;
      case ')':
        begin_token(&t, TokenIdRParen);
        end_token(&t);
        break;
      case ',':
        begin_token(&t, TokenIdComma);
        end_token(&t);
        break;
      case '*':
        begin_token(&t, TokenIdStar);
        end_token(&t);
        break;
      case '%':
        begin_token(&t, TokenIdPercent);
        end_token(&t);
        break;
      case '{':
        begin_token(&t, TokenIdLBrace);
        end_token(&t);
        break;
      case '}':
        begin_token(&t, TokenIdRBrace);
        end_token(&t);
        break;
      case ';':
        begin_token(&t, TokenIdSemicolon);
        end_token(&t);
        break;
      case ':':
        begin_token(&t, TokenIdColon);
        end_token(&t);
        break;
      case '+':
        begin_token(&t, TokenIdPlus);
        end_token(&t);
        break;
      case '~':
        begin_token(&t, TokenIdTilde);
        end_token(&t);
        break;
      case '-':
        begin_token(&t, TokenIdDash);
        t.state = TokenizeStateSawDash;
        break;
      case '#':
        begin_token(&t, TokenIdNumberSign);
        end_token(&t);
        break;
      case '^':
        begin_token(&t, TokenIdBinXor);
        end_token(&t);
        break;
      case '/':
        begin_token(&t, TokenIdSlash);
        t.state = TokenizeStateSawSlash;
        break;
      case '|':
        begin_token(&t, TokenIdBinOr);
        t.state = TokenizeStatePipe;
        break;
      case '&':
        begin_token(&t, TokenIdBinAnd);
        t.state = TokenizeStateAmpersand;
        break;
      case '=':
        begin_token(&t, TokenIdEq);
        t.state = TokenizeStateEq;
        break;
      case '!':
        begin_token(&t, TokenIdBang);
        t.state = TokenizeStateBang;
        break;
      case '<':
        begin_token(&t, TokenIdCmpLessThan);
        t.state = TokenizeStateLessThan;
        break;
      case '>':
        begin_token(&t, TokenIdCmpGreaterThan);
        t.state = TokenizeStateGreaterThan;
        break;
      default:
        tokenize_error(&t, "invalid character: '%c'", c);
      }
      break;
    case TokenizeStateGreaterThan:
      switch (c) {
      case '=':
        t.cur_tok->id = TokenIdCmpGreaterOrEq;
        end_token(&t);
        t.state = TokenizeStateStart;
        break;
      case '>':
        t.cur_tok->id = TokenIdBitShiftRight;
        end_token(&t);
        t.state = TokenizeStateStart;
        break;
      default:
        t.pos -= 1;
        end_token(&t);
        t.state = TokenizeStateStart;
        continue;
      }
      break;
    case TokenizeStateLessThan:
      switch (c) {
      case '=':
        t.cur_tok->id = TokenIdCmpLessOrEq;
        end_token(&t);
        t.state = TokenizeStateStart;
      case '<':
        t.cur_tok->id = TokenIdBitShiftLeft;
        end_token(&t);
        t.state = TokenizeStateStart;
        break;
      default:
        t.pos -= 1;
        end_token(&t);
        t.state = TokenizeStateStart;
        continue;
      }
      break;
    case TokenizeStateBang:
      switch (c) {
      case '=':
        t.cur_tok->id = TokenIdCmpNotEq;
        end_token(&t);
        t.state = TokenizeStateStart;
        break;
      default:
        t.pos -= 1;
        end_token(&t);
        t.state = TokenizeStateStart;
        continue;
      }
      break;
    case TokenizeStateEq:
      switch (c) {
      case '=':
        t.cur_tok->id = TokenIdCmpEq;
        end_token(&t);
        t.state = TokenizeStateStart;
        break;
      default:
        t.pos -= 1;
        end_token(&t);
        t.state = TokenizeStateStart;
        continue;
      }
      break;
    case TokenizeStateAmpersand:
      switch (c) {
      case '&':
        t.cur_tok->id = TokenIdBoolAnd;
        end_token(&t);
        t.state = TokenizeStateStart;
        break;
      default:
        t.pos -= 1;
        end_token(&t);
        t.state = TokenizeStateStart;
        continue;
      }
      break;
    case TokenizeStatePipe:
      switch (c) {
      case '|':
        t.cur_tok->id = TokenIdBoolOr;
        end_token(&t);
        t.state = TokenizeStateStart;
        break;
      default:
        t.pos -= 1;
        end_token(&t);
        t.state = TokenizeStateStart;
        continue;
      }
      break;
    case TokenizeStateSawSlash:
      switch (c) {
      case '/':
        cancel_token(&t);
        t.state = TokenizeStateLineComment;
        break;
      case '*':
        cancel_token(&t);
        t.state = TokenizeStateMultiLineComment;
        t.multi_line_comment_count = 1;
        break;
      default:
        end_token(&t);
        t.state = TokenizeStateStart;
        break;
      }
      break;
    case TokenizeStateLineComment:
      switch (c) {
      case '\n':
        t.state = TokenizeStateStart;
        break;
      default:
        break;
      }
      break;
    case TokenizeStateMultiLineComment:
      switch (c) {
      case '*':
        t.state = TokenizeStateMultiLineCommentStar;
        break;
      case '/':
        t.state = TokenizeStateMultiLineCommentSlash;
        break;
      default:
        break;
      }
      break;
    case TokenizeStateMultiLineCommentSlash:
      switch (c) {
      case '*':
        t.state = TokenizeStateMultiLineComment;
        t.multi_line_comment_count += 1;
        break;
      case '/':
        break;
      default:
        t.state = TokenizeStateMultiLineComment;
        break;
      }
      break;
    case TokenizeStateMultiLineCommentStar:
      switch (c) {
      case '/':
        t.multi_line_comment_count -= 1;
        if (t.multi_line_comment_count == 0) {
          t.state = TokenizeStateStart;
        } else {
          t.state = TokenizeStateMultiLineComment;
        }
        break;
      case '*':
        break;
      default:
        t.state = TokenizeStateMultiLineComment;
        break;
      }
      break;
    case TokenizeStateSymbol:
      switch (c) {
      case SYMBOL_CHAR:
        break;
      default:
        t.pos -= 1;
        end_token(&t);
        t.state = TokenizeStateStart;
        continue;
      }
      break;
    case TokenizeStateString:
      switch (c) {
      case '"':
        end_token(&t);
        t.state = TokenizeStateStart;
        break;
      default:
        break;
      }
      break;
    case TokenizeStateNumber:
      switch (c) {
      case DIGIT:
        break;
      default:
        t.pos -= 1;
        end_token(&t);
        t.state = TokenizeStateStart;
        continue;
      }
      break;
    case TokenizeStateSawDash:
      switch (c) {
      case '>':
        t.cur_tok->id = TokenIdArrow;
        end_token(&t);
        t.state = TokenizeStateStart;
        break;
      default:
        end_token(&t);
        t.state = TokenizeStateStart;
        break;
      }
      break;
    }
    if (c == '\n') {
      t.line += 1;
      t.column = 0;
    } else {
      t.column += 1;
    }
  }
  switch (t.state) {
  case TokenizeStateStart:
    break;
  case TokenizeStateString:
    tokenize_error(&t, "unterminated string");
    break;
  case TokenizeStateSymbol:
  case TokenizeStateNumber:
  case TokenizeStateSawDash:
  case TokenizeStatePipe:
  case TokenizeStateAmpersand:
  case TokenizeStateEq:
  case TokenizeStateBang:
  case TokenizeStateLessThan:
  case TokenizeStateGreaterThan:
    end_token(&t);
    break;
  case TokenizeStateSawSlash:
    tokenize_error(&t, "unexpected EOF");
    break;
  case TokenizeStateLineComment:
    break;
  case TokenizeStateMultiLineComment:
  case TokenizeStateMultiLineCommentSlash:
  case TokenizeStateMultiLineCommentStar:
    tokenize_error(&t, "unterminated multi-line commend");
    break;
  }
  t.pos = -1;
  begin_token(&t, TokenIdEof);
  end_token(&t);
  assert(!t.cur_tok);
  return t.tokens;
}

static const char *token_name(Token *token) {
  switch (token->id) {
  case TokenIdEof:
    return "EOF";
  case TokenIdSymbol:
    return "Symbol";
  case TokenIdKeywordFn:
    return "Fn";
  case TokenIdKeywordConst:
    return "Const";
  case TokenIdKeywordMut:
    return "Mut";
  case TokenIdKeywordReturn:
    return "Return";
  case TokenIdKeywordExtern:
    return "Extern";
  case TokenIdKeywordUnreachable:
    return "Unreachable";
  case TokenIdKeywordPub:
    return "Pub";
  case TokenIdKeywordExport:
    return "Export";
  case TokenIdKeywordAs:
    return "As";
  case TokenIdKeywordUse:
    return "Use";
  case TokenIdLParen:
    return "LParen";
  case TokenIdRParen:
    return "RParen";
  case TokenIdComma:
    return "Comma";
  case TokenIdStar:
    return "Star";
  case TokenIdLBrace:
    return "LBrace";
  case TokenIdRBrace:
    return "RBrace";
  case TokenIdStringLiteral:
    return "StringLiteral";
  case TokenIdSemicolon:
    return "Semicolon";
  case TokenIdNumberLiteral:
    return "NumberLiteral";
  case TokenIdPlus:
    return "Plus";
  case TokenIdColon:
    return "Colon";
  case TokenIdArrow:
    return "Arrow";
  case TokenIdDash:
    return "Dash";
  case TokenIdNumberSign:
    return "NumberSign";
  case TokenIdBinOr:
    return "BinOr";
  case TokenIdBinAnd:
    return "BinAnd";
  case TokenIdBinXor:
    return "BinXor";
  case TokenIdBoolOr:
    return "BoolOr";
  case TokenIdBoolAnd:
    return "BoolAnd";
  case TokenIdEq:
    return "Eq";
  case TokenIdBang:
    return "Bang";
  case TokenIdTilde:
    return "Tilde";
  case TokenIdCmpEq:
    return "CmpEq";
  case TokenIdCmpNotEq:
    return "CmpNotEq";
  case TokenIdCmpLessThan:
    return "CmpLessThan";
  case TokenIdCmpGreaterThan:
    return "CmpGreaterThan";
  case TokenIdCmpLessOrEq:
    return "CmpLessOrEq";
  case TokenIdCmpGreaterOrEq:
    return "CmpGreaterOrEq";
  case TokenIdBitShiftLeft:
    return "BitShiftLeft";
  case TokenIdBitShiftRight:
    return "BitShiftRight";
  case TokenIdSlash:
    return "Slash";
  case TokenIdPercent:
    return "Percent";
  }
  return "(invalid token)";
}

void print_tokens(Buf *buf, JaneList<Token> *tokens) {
  for (int i = 0; i < tokens->length; i += 1) {
    Token *token = &tokens->at(i);
    printf("%s ", token_name(token));
    fwrite(buf_ptr(buf) + token->start_position, 1,
           token->end_position - token->start_position, stdout);
    printf("\n");
  }
}
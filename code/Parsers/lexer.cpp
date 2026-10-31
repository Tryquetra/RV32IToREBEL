// Original made by Austin Henley and copied from https://github.com/AZHenley/riscv-parser, then modified

#include <string>
#include "lexer.h"
#include "token.h"

#include <errors.h>

#include <memory>

#define EOFConstant 3 // Using 0 is annoying for debugging.

Lexer::Lexer(const std::string& source)
   : m_source(source)
   , m_ch(0)
   , m_pos(0)
   , m_line(1)
   , m_column(1)
{
   nextChar(); // Initialize lexer with first character.
}

void Lexer::nextChar()
{
   if(m_ch == '\n')
   {
      ++m_line;
      m_column = 1;
   }
   else if((m_ch != 0) && (m_ch != EOFConstant))
   {
      ++m_column;
   }

   if(m_pos >= m_source.length())
   {
      m_ch = EOFConstant;
   }
   else
   {
      m_ch = m_source[m_pos];
   }
   m_pos++;
}

char Lexer::peekChar()
{
   if(m_pos >= m_source.length())
   {
      return EOFConstant;
   }
   return m_source[m_pos];
}

Token* Lexer::nextToken()
{
   auto token = std::make_unique<Token>();
   Token* t = token.get();
   skipSpaces();
   t->line = m_line;
   t->column = m_column;
   t->literal = std::string(1, m_ch);

   switch(m_ch)
   {
   case '\r': case '\n': case ';':
      t->type = TokenType::Newline;
      // Reduce consecutive newlines to one (even if spaces are between them).
      while(isNewline() || isSpace())
         nextChar();
      break;
   case '+':
      t->type = TokenType::Plus;
      nextChar();
      break;
   case '-':
      t->type = TokenType::Minus;
      nextChar();
      break;
   case '(':
      t->type = TokenType::Lparen;
      nextChar();
      break;
   case ')':
      t->type = TokenType::Rparen;
      nextChar();
      break;
   case '.':
      t->type = TokenType::Dot;
      nextChar();
      break;
   case ',':
      t->type = TokenType::Comma;
      nextChar();
      break;
   case ':':
      t->type = TokenType::Colon;
      nextChar();
      break;
   case '#':
      t->type = TokenType::Comment;
      nextChar();
      while(!isNewline() && (m_ch != EOFConstant) && (m_ch != 0))
      { // Continue until newline or end of file.
         t->literal += m_ch;
         nextChar();
      }
      break;
   case EOFConstant: case 0:
      t->type = TokenType::EndOfFile;
      nextChar(); // Shouldn't matter.
      break;
   default:
      t->type = TokenType::Symbol;
      // Either an instruction name, directive name, string literal, or number.
      if(m_ch == '"')
      {
         // String literal.
         nextChar();
         while((m_ch != '"') && (m_ch != EOFConstant) && (m_ch != 0))
         {
            t->literal += m_ch;
            nextChar();
         }
         if((m_ch != '"'))
         {
            throw RebelError("unterminated string literal", {t->line, t->column});
         }
         t->literal += m_ch; // Include ending quotation mark.
         nextChar();
      }
      else
      {
         nextChar();
         while(isAlpha() || isNum() || isPunct())
         {
            t->literal += m_ch;
            nextChar();
         }
      }
   }
   return token.release();
}

bool Lexer::isPunct()
{
   // Some assemblers allow $`._'
   return m_ch == 36 || m_ch == 39 || m_ch == 46 || m_ch == 95 || m_ch == 96;
}

bool Lexer::isAlpha()
{
   return (m_ch >= 65 && m_ch <= 90) || (m_ch >= 97 && m_ch <= 122);
}

bool Lexer::isNum()
{
   return m_ch >= 48 && m_ch <= 57;
}

bool Lexer::isNewline()
{
   return m_ch == '\r' || m_ch == '\n' || m_ch == ';';
}

bool Lexer::isSpace()
{
   return m_ch == ' ' || m_ch == '\t';
}

void Lexer::skipSpaces()
{
   while(isSpace())
   {
      nextChar();
   }
}

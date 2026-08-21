#include "FrontEnd.h"
#include <catch2/matchers/catch_matchers_string.hpp>
#include "ParseError.h"
#include "ParserHelper.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("TOP Parser: conditionals", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      short() {
        var x, y, z;
        if (x>0) {
          while (y>z) {
            y = y + 1;
          }
        } else {
          z = z + 1;
        }
        return z;
      }
    )";

  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: operators", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      operators() {
        var x;
        x = y + 1;
        x = y - 1;
        x = y * 1;
        x = y / 1;
        x = -1;
        x = 1 > 0;
        x = 1 == 0;
        x = 1 != 0;
        return z;
      }
    )";

  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: pointers", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      operators() {
        var x, y;
        y = alloc 13;
        x = &y;
        *y = 42;
        **x = 7;
        return *y;
      }
    )";

  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: null literal rejected", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      main() {
        var x;
        x = null;
        return 0;
      }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: funs", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      foo(f, a) { return f(a); }
      bar(x) { return x + 1; }
      baz(y) { return foo(bar, y); }
      main(z) { return baz(z); }
    )";

  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: decls", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      main() { var x; var y; var z; return 0; }
    )";

  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: parens", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      main() { return ((1 + 2) * 3) - ((((2 - 1)))); }
    )";

  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: io stmts", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      main() { var x; x = input; output x; error x; output x * x; error (x * x); return x; }
    )";

  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: block stmts", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      main() { var x, y; { x = 0; { y = x + 1; } } return x + y; }
    )";

  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: identifiers and literals", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      main() { var a_314, b_; a_314 = 00007; b_ = 0000; return b_; }
    )";

  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: dangling else", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      operators() { var x; if (x==0) if (x==0) x = x + 1; else x = x-1; return x; }
    )";

  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: input", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      operators() { var x; if (input) if (input) x = 1; else x = -1; return x; }
      outin() { output input; return 0; }
    )";

  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: nested returns are rejected", "[TOP Parser]") {
  // A 'return' may appear only as a function's final statement.  Nested or
  // early returns are not part of the grammar, so they fail to parse.
  std::stringstream stream;
  stream << R"(
      main() {
        var x;
        x = 1;
        if (x) {
          return x;
        }
        return x;
      }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

/* These tests checks for operator precedence. */
TEST_CASE("TOP Parser: mul higher precedence than add", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(main() { return 1 + 2 * 3; })";
  std::string expected = "(expr (expr 1) + (expr (expr 2) * (expr 3)))";
  std::string tree = ParserHelper::parsetree(stream);
  REQUIRE(tree.find(expected) != std::string::npos);
}

TEST_CASE("TOP Parser: fun app higher precedence than deref", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(main() { var p; return *p(); })";
  std::string expected = "(expr * (expr (expr p) ( )))";
  std::string tree = ParserHelper::parsetree(stream);
  REQUIRE(tree.find(expected) != std::string::npos);
}

TEST_CASE("TOP Parser: anonymous record expression rejected", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      main() {
        var r, x;
        r = {a:1, b:x};
        x = r.a;
        return x;
      }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: field access expression rejected", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(main() { var n; return *n.p; })";
  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: record pattern rejected", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
    type MaybePoint = Just(p) | Nothing;
    main() {
      var p;
      case p of { Just({x: v}) -> return v; Nothing -> return 0; }
    }
  )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

/************ The following are expected to fail parsing ************/

TEST_CASE("TOP Parser: decl after stmt", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      main() { var x; x = 0; var z; return 0; }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: missing semi-colon", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      short() { var x; if (x>0) x = x + 1 return 0; }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: missing paren", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      short() { var x; if x>0 x = x + 1; return 0; }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: unbalanced blocks", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      main() { var x, y; { x = 0; y = x + 1; } } return x + y; }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: unbalanced binary expr", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      operators() { var x; x = y + + 1; return -x; }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: bad field delimiter", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      operators() { var x; x = {a:0, b 0}; return x.a; }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: bad field separator", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      operators() { var x; x = {a:0 b:0}; return x.a; }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: no expression statements", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      operators() { var x, y; x = y = 1; return x; }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

/************ TOP extension parser tests ************/

TEST_CASE("TOP Parser: sum type decl single variant no payload", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      type Color = Red | Green | Blue;
      main() { return 0; }
    )";
  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: sum type decl with payload variants", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      type Shape = Circle(r) | Rect(w, h) | Point;
      main() { return 0; }
    )";
  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: sum type decl single variant with payload", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      type Box = Wrap(val);
      main() { return 0; }
    )";
  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: case stmt single arm", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      type Color = Red | Green | Blue;
      f(x) {
        var result;
        case x of { Red -> result = 0; }
        return result;
      }
    )";
  REQUIRE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: case stmt multiple arms with payload", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      type Shape = Circle(r) | Rect(w, h);
      area(s) {
        var result;
        case s of {
          Circle(r) -> result = r * r;
          Rect(w, h) -> result = w * h;
        }
        return result;
      }
    )";
  REQUIRE(ParserHelper::is_parsable(stream));
}

/*** Negative TOP parser tests ***/

TEST_CASE("TOP Parser: reject case missing of", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      type C = A | B;
      f(x) { case x { A -> return 1; } return 0; }
    )";
  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: reject sum type decl missing equals", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      type Color Red | Green | Blue;
      main() { return 0; }
    )";
  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: reject lowercase type name", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      type color = Red | Green;
      main() { return 0; }
    )";
  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: reject lowercase constructor in type decl", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      type Color = red | Green;
      main() { return 0; }
    )";
  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: reject lowercase constructor in case arm", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      type C = A | B;
      f(x) { var r; case x of { a -> r = 1; } return r; }
    )";
  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Parser: keywords as ids", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      if() { var x; if (x <= 0) x = x + 1; return x; }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Lexer: illegal comparison token", "[TOP Lexer]") {
  std::stringstream stream;
  stream << R"(
      operators() { var x; if (x <= 0) x = x + 1; return x; }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Lexer: illegal operator token", "[TOP Lexer]") {
  std::stringstream stream;
  stream << R"(
      operators() { var x; if (x == 0) x = x % 2; return x; }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Lexer: illegal identifier token", "[TOP Lexer]") {
  std::stringstream stream;
  stream << R"(
      operators() { var $x; if ($x == 0) $x = $x + 2; return $x; }
    )";

  REQUIRE_FALSE(ParserHelper::is_parsable(stream));
}

TEST_CASE("TOP Lexer: Lexing exceptions are thrown", "[TOP Lexer]") {
  std::stringstream stream;
  stream << R"(
      main() {
        return ";
      }
    )";

  REQUIRE_THROWS_WITH(FrontEnd::parse(stream),
                         Catch::Matchers::ContainsSubstring("token recognition error"));
}

TEST_CASE("TOP Parser: Parsing exceptions are thrown", "[TOP Parser]") {
  std::stringstream stream;
  stream << R"(
      main() {
        return 0
      }
    )";

  REQUIRE_THROWS_WITH(FrontEnd::parse(stream),
                         Catch::Matchers::ContainsSubstring("missing ';'"));
}

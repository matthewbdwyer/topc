#include "TermPrint.h"

namespace verutop {

// ---- SMT-LIB --------------------------------------------------------------

void smt(const T &t, std::string &o) {
  auto apply = [&](const std::string &head) {
    if (t->args.empty()) {
      o += head;
      return;
    }
    o += "(" + head;
    for (auto &a : t->args) {
      o += " ";
      smt(a, o);
    }
    o += ")";
  };
  switch (t->kind) {
  case Term::Num:
    o += t->num < 0 ? "(- " + std::to_string(-t->num) + ")" : std::to_string(t->num);
    return;
  case Term::Var: o += "|" + t->name + "|"; return;
  case Term::Op: apply(t->name); return;
  case Term::Fn:
  case Term::Ctor:
  case Term::Sel: apply("|" + t->name + "|"); return;
  case Term::Is: apply("(_ is |" + t->name + "|)"); return;
  case Term::Let:
    o += "(let ((|" + t->name + "| ";
    smt(t->args[0], o);
    o += ")) ";
    smt(t->args[1], o);
    o += ")";
    return;
  case Term::Forall:
    o += "(forall (";
    for (size_t i = 0; i < t->bound.size(); ++i)
      o += "(|" + t->bound[i] + "| " + (i < t->sorts.size() ? t->sorts[i] : "Int") + ")";
    o += ") ";
    smt(t->args[0], o);
    o += ")";
    return;
  }
}
std::string smt(const T &t) {
  std::string o;
  smt(t, o);
  return o;
}

// ---- infix ----------------------------------------------------------------

bool isCmp(const std::string &o) {
  return o == "=" || o == "<" || o == "<=" || o == ">" || o == ">=";
}
int prec(const T &t) {
  if (t->kind == Term::Let || t->kind == Term::Forall) return 0;
  if (t->kind == Term::Is) return 5;
  if (t->kind != Term::Op || t->args.empty()) return 9;
  auto &o = t->name;
  if (o == "=>") return 1;
  if (o == "or") return 2;
  if (o == "and") return 3;
  if (o == "not") return isOp(t->args[0], "=") ? 5 : 8;
  if (isCmp(o)) return 5;
  if (o == "+" || o == "-") return 6;
  if (o == "*" || o == "tdiv") return 7;
  return 9;
}
std::string sym(const std::string &o) {
  if (o == "=") return "==";
  if (o == "and") return "&&";
  if (o == "or") return "||";
  if (o == "=>") return "==>";
  if (o == "tdiv") return "/";
  return o;
}

std::string flat(const T &t);
std::string paren(const T &c, bool need) {
  auto s = flat(c);
  return need ? "(" + s + ")" : s;
}
std::string args(const T &t) {
  std::string s = "(";
  for (size_t i = 0; i < t->args.size(); ++i) s += (i ? ", " : "") + flat(t->args[i]);
  return s + ")";
}
std::string flat(const T &t) {
  switch (t->kind) {
  case Term::Num: return std::to_string(t->num);
  case Term::Var: return t->name;
  case Term::Fn: return (t->name.rfind("rank#", 0) == 0 ? "rank" : t->name) + args(t);
  case Term::Ctor: return t->args.empty() ? t->name : t->name + args(t);
  case Term::Is: return paren(t->args[0], prec(t->args[0]) <= 5) + " is " + t->name;
  case Term::Sel:
    return paren(t->args[0], prec(t->args[0]) < 9) + "." + t->name.substr(t->name.find('.') + 1);
  case Term::Let:
    return "let " + t->name + " = " + flat(t->args[0]) + " in " + flat(t->args[1]);
  case Term::Forall: {
    std::string s = "forall ";
    for (size_t i = 0; i < t->bound.size(); ++i) s += (i ? ", " : "") + t->bound[i];
    return s + " :: " + flat(t->args[0]);
  }
  case Term::Op: break;
  }
  auto &o = t->name;
  auto &a = t->args;
  if (a.empty()) return o;
  if (o == "not") {
    if (isOp(a[0], "=")) {
      auto &e = a[0]->args;
      return paren(e[0], prec(e[0]) <= 5) + " != " + paren(e[1], prec(e[1]) <= 5);
    }
    return "!" + paren(a[0], prec(a[0]) < 8);
  }
  if (o == "ite") return "(" + flat(a[0]) + " ? " + flat(a[1]) + " : " + flat(a[2]) + ")";
  int p = prec(t);
  if (o == "and" || o == "or") {
    std::string s;
    for (size_t i = 0; i < a.size(); ++i)
      s += (i ? " " + sym(o) + " " : "") + paren(a[i], prec(a[i]) <= p);
    return s;
  }
  if (o == "=>") return paren(a[0], prec(a[0]) <= 1) + " ==> " + paren(a[1], prec(a[1]) < 1);
  bool assoc = o == "+" || o == "*";
  return paren(a[0], isCmp(o) ? prec(a[0]) <= p : prec(a[0]) < p) + " " + sym(o) + " " +
         paren(a[1], assoc ? prec(a[1]) < p : prec(a[1]) <= p);
}

// Infix over several lines: a formula wider than `width` is broken at its
// top-level connectives.
std::vector<std::string> pretty(const T &t, int width) {
  if (width < 24) width = 24;
  auto s = flat(t);
  if ((int)s.size() <= width) return {s};
  auto child = [&](const T &c, bool need, int w) {
    auto L = pretty(c, need ? w - 2 : w);
    if (need) {
      L.front() = "(" + L.front();
      for (size_t i = 1; i < L.size(); ++i) L[i] = " " + L[i];
      L.back() += ")";
    }
    return L;
  };
  std::vector<std::string> out;
  if (isOp(t, "and") || isOp(t, "or")) {
    for (size_t i = 0; i < t->args.size(); ++i) {
      auto L = child(t->args[i], prec(t->args[i]) <= prec(t), width);
      if (i + 1 < t->args.size()) L.back() += " " + sym(t->name);
      out.insert(out.end(), L.begin(), L.end());
    }
    return out;
  }
  if (isOp(t, "=>")) {
    out = child(t->args[0], prec(t->args[0]) <= 1, width);
    out.back() += " ==>";
    for (auto &l : child(t->args[1], prec(t->args[1]) < 1, width - 2)) out.push_back("  " + l);
    return out;
  }
  if (t->kind == Term::Let) {
    out.push_back("let " + t->name + " = " + flat(t->args[0]) + " in");
    for (auto &l : pretty(t->args[1], width)) out.push_back(l);
    return out;
  }
  if (t->kind == Term::Forall) {
    std::string h = "forall ";
    for (size_t i = 0; i < t->bound.size(); ++i) h += (i ? ", " : "") + t->bound[i];
    out.push_back(h + " ::");
    for (auto &l : pretty(t->args[0], width - 2)) out.push_back("  " + l);
    return out;
  }
  return {s};
}

} // namespace verutop

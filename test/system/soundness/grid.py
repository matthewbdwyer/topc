"""Bounded-exhaustive ownership soundness grid for topc.

Each case is a small TOP program that applies one or two operations to a
single owner in a given context. The expected verdict is derived here, from
the ownership rules, *before* the compiler is run (see `expect`):

  D1  destruction is static: ownership state at every point is the same on
      every path (joins agree, loop condition and body are invariant, moves
      take effect in evaluation order);
  D2  borrows are call-bounded and non-invalidating: a call may not move an
      owner that one of its actuals borrows;
  D3  a generic body is compiled once: an owned instance must be passed on
      exactly once and not used after.

The runner (run_grid.py, or run.py's `soundness` category) compiles every
case with --san, runs accepted ones under ASan/LSan with main(c) for c = 0
and c = 1, and classifies the outcome:

  OK        compiler and model agree, and accepted programs run clean
  UNSOUND   accepted, but a sanitizer report or a nonzero exit
  SLACK     model accepts, compiler rejects (a restriction to review)
  PERMISSIVE model rejects, compiler accepts and runs clean (model or
            compiler to review)
  DIAG      both reject, but the diagnostic is not one the model expects
  ILLTYPED  rejected by type inference: the cell is not a TOP program
"""

from dataclasses import dataclass, field
from typing import List, Optional

# ---------------------------------------------------------------------------
# Owner kinds
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Kind:
    name: str
    owned: bool          # classifies Own
    types: str           # type declarations
    init: str            # expression that makes the owner
    helpers: str         # consume(p) and look(q)
    ops: tuple           # operation names that apply


KINDS = [
    Kind("int", False, "",
         "5",
         "consume(p) { return p + 1; }\nlook(q) { return *q; }\n",
         ("assign", "consume", "ident", "borrow", "payload")),
    Kind("own", True, "",
         "alloc 5",
         "consume(p) { return *p; }\nlook(q) { return **q; }\n"
         "make() { var p; p = alloc 7; return p; }\n",
         ("assign", "consume", "ident", "borrow", "read", "write", "payload",
          "temp")),
    Kind("sum", True, "type Cell = Val(n) | Nope;\n",
         "Val(5)",
         "consume(p) { var t; t = 0; case p of { Val(n) -> t = n; Nope -> t = 0; } return t; }\n"
         "look(q) { var t; t = 0; case *q of { Val(n) -> t = n; Nope -> t = 0; } return t; }\n",
         ("assign", "consume", "ident", "borrow", "casev", "payload")),
    Kind("sumown", True, "type Box = Full(v) | Empty;\n",
         "Full(alloc 5)",
         "consume(p) { var t; t = 0; case p of { Full(v) -> t = *v; Empty -> t = 0; } return t; }\n"
         "look(q) { var t; t = 0; case *q of { Full(v) -> t = *v; Empty -> t = 0; } return t; }\n",
         ("assign", "consume", "ident", "borrow", "casev", "payload")),
]

# ---------------------------------------------------------------------------
# Operations on the owner `x`
# ---------------------------------------------------------------------------
# category: "move" (takes ownership) or "use" (reads, borrows, writes)
# stmt:     the operation as a statement
# expr:     the operation as an int-valued expression, if it has one
# arg:      (actual, helper body using formal `a`) to pass it to a call, if any
# creates:  the variable it makes an owner of ("y"/"w"), given the kind


@dataclass(frozen=True)
class Op:
    name: str
    category: str
    stmt: str
    expr: Optional[str]
    arg: Optional[tuple]


def ops_for(kind: Kind):
    casev = {
        "sum": "case x of { Val(n) -> r = r + n; Nope -> r = r + 0; }",
        "sumown": "case x of { Full(v) -> r = r + *v; Empty -> r = r + 0; }",
    }
    table = {
        "assign": Op("assign", "move", "y = x;", None, None),
        "consume": Op("consume", "move", "r = r + consume(x);", "consume(x)",
                      ("x", "consume(a)")),
        "ident": Op("ident", "move", "y = ident(x);", None, None),
        "borrow": Op("borrow", "use", "r = r + look(&x);", "look(&x)",
                     ("&x", "look(a)")),
        "read": Op("read", "use", "r = r + *x;", "*x", ("*x", "a")),
        "write": Op("write", "use", "*x = 3;", None, None),
        "casev": Op("casev", "move", casev.get(kind.name, ""), None, None),
        "payload": Op("payload", "move", "w = Wrap(x);", None, None),
        # An owned result dereferenced without being stored (independent of x).
        "temp": Op("temp", "use", "r = r + *make();", "*make()", ("*make()", "a")),
    }
    return [table[n] for n in kind.ops]


def creates(op: Op, kind: Kind) -> Optional[str]:
    """The variable an operation makes an owner of, if any."""
    if op.name == "payload":
        return "w"  # a Wrapper box always owns
    if op.name in ("assign", "ident") and kind.owned:
        return "y"
    return None


def is_move(op: Op, kind: Kind) -> bool:
    return kind.owned and op.category == "move"


def touches_owner(op: Optional[Op]) -> bool:
    """Whether an operation involves the owner x at all."""
    return op is not None and op.name != "temp"


# ---------------------------------------------------------------------------
# Contexts
# ---------------------------------------------------------------------------

CONTEXTS = ["seq", "expr", "call", "if1", "if2", "loop", "cond", "arm1", "gen",
            "shadow"]


@dataclass
class Case:
    name: str
    kind: str
    context: str
    op1: str
    op2: str
    source: str
    expect: str                      # "accept" or "reject"
    reasons: List[str] = field(default_factory=list)  # acceptable substrings


# Diagnostic families, as substrings of topc messages.
USE_AFTER_MOVE = ["used after move", "moved more than once"]
JOIN = ["ownership state disagreement at control-flow join"]
LOOP = ["move inside while-loop body", "still owned at the end of a while-loop"]
COND = ["move in while-loop condition"]
HELD = ["borrows it; a borrowed owner must stay alive"]
OVERWRITE_LIVE = ["assigned while still owned"]
GENERIC_REUSE = ["which uses it again after passing it on"]
GENERIC_DROP = ["neither returned nor borrowed nor passed on"]


def expect(kind: Kind, context: str, op1: Op, op2: Optional[Op]):
    """The verdict the rules give, and the diagnostics that may report it."""
    if context == "gen":
        # op1/op2 are "pass" (f(z)) or "lend" (g(&z)) inside a generic body.
        if not kind.owned:
            return "accept", []
        passes = [op1.name, op2.name].count("pass")
        if passes == 0:
            return "reject", GENERIC_DROP
        if op1.name == "pass":
            return "reject", GENERIC_REUSE   # used again after passing on
        return "accept", []

    c1 = creates(op1, kind)
    c2 = creates(op2, kind) if op2 else None

    if context == "shadow":
        # op1 then op2 inside both arms of an enclosing by-value match whose
        # binder has the same name as the owner-case binder (n / v): the same
        # verdict as sequencing, since every arm runs both.
        context = "seq"

    if context in ("if1", "loop", "arm1"):
        # op1 runs on some paths or iterations only.
        if is_move(op1, kind) or c1:
            return "reject", (LOOP if context == "loop" else JOIN)
        return "accept", []

    if context == "cond":
        if is_move(op1, kind):
            return "reject", COND
        return "accept", []

    if context == "call":
        m1, m2 = is_move(op1, kind), is_move(op2, kind)
        if m1 and m2:
            return "reject", USE_AFTER_MOVE
        if (m1 and op2.name == "borrow") or (m2 and op1.name == "borrow"):
            return "reject", HELD
        if m1 and op2.category == "use" and touches_owner(op2):
            return "reject", USE_AFTER_MOVE   # evaluated after the move
        return "accept", []

    # seq, if2, expr: op1 then op2 on every path, in order.
    if touches_owner(op2) and is_move(op1, kind):
        return "reject", USE_AFTER_MOVE + OVERWRITE_LIVE
    if c1 and c2 and c1 == c2:
        return "reject", OVERWRITE_LIVE
    return "accept", []


# ---------------------------------------------------------------------------
# Program construction
# ---------------------------------------------------------------------------

PRELUDE = """ident(z) { return z; }
"""


def program(kind: Kind, context: str, op1: Op, op2: Optional[Op]) -> Optional[str]:
    types = kind.types + "type Wrapper = Wrap(inner);\ntype Sel = On(k) | Off;\n"
    helpers = PRELUDE + kind.helpers
    s1 = op1.stmt
    s2 = op2.stmt if op2 else ""
    extra = ""

    if context == "seq":
        body = f"{s1}\n  {s2}"
    elif context == "if2":
        body = f"if (c > 0) {{ {s1} }} else {{ {s1} }}\n  {s2}"
    elif context == "if1":
        body = f"if (c > 0) {{ {s1} }}\n  {s2}"
    elif context == "loop":
        body = f"i = 2;\n  while (i > 0) {{ {s1} i = i - 1; }}\n  {s2}"
    elif context == "arm1":
        body = (f"sel = On(1);\n  case sel of {{ On(k) -> {{ {s1} }} Off -> r = r + 0; }}\n  {s2}")
    elif context == "cond":
        if op1.expr is None:
            return None
        body = f"i = 0;\n  while ({op1.expr} > i) {{ i = i + 1000; }}\n  {s2}"
    elif context == "expr":
        if op1.expr is None or op2 is None or op2.expr is None:
            return None
        body = f"r = r + {op1.expr} + {op2.expr};"
    elif context == "call":
        if op1.arg is None or op2 is None or op2.arg is None:
            return None
        extra = f"pair(a1, a2) {{ return {op1.arg[1].replace('a', 'a1', 1) if op1.arg[1] != 'a' else 'a1'} + {op2.arg[1].replace('a', 'a2', 1) if op2.arg[1] != 'a' else 'a2'}; }}\n"
        body = f"r = r + pair({op1.arg[0]}, {op2.arg[0]});"
    elif context == "shadow":
        if kind.name not in ("sum", "sumown"):
            return None
        outer = "Val" if kind.name == "sum" else "Full"
        other = "Nope" if kind.name == "sum" else "Empty"
        name = "n" if kind.name == "sum" else "v"
        init = "Val(9)" if kind.name == "sum" else "Full(alloc 9)"
        use = f"r = r + {name};" if kind.name == "sum" else f"r = r + *{name};"
        body = (f"y = {init};\n  case y of {{ {outer}({name}) -> {{ {s1} {s2} {use} }} "
                f"{other} -> {{ {s1} {s2} }} }}")
        if "y = " in s1 or "y = " in s2:
            return None  # y is the enclosing scrutinee here
    elif context == "gen":
        stmts = {"pass": "t = t + f(z);", "lend": "t = t + g(&z);"}
        extra = (f"gen(f, g, z) {{ var t; t = 0; {stmts[op1.name]} "
                 f"{stmts[op2.name]} return t; }}\n")
        lender = "look" if "lend" in (op1.name, op2.name) else "consume"
        body = f"r = r + gen(consume, {lender}, x);"
    else:
        raise ValueError(context)

    # Emit only the helpers the program uses: an unused generic helper keeps a
    # fully generic type and can be rejected at its definition on its own.
    used = body + extra
    kept = []
    for line in helpers.splitlines(keepends=True):
        fname = line.split("(", 1)[0].strip()
        if fname and (fname + "(") in used or (fname == "consume" and "consume" in used) \
                or (fname == "look" and "look" in used):
            kept.append(line)
    helpers = "".join(kept)
    return (f"{types}{helpers}{extra}"
            f"main(c) {{\n  var x, y, w, r, i, sel;\n  r = 0;\n  x = {kind.init};\n"
            f"  {body}\n  return 0;\n}}\n")


def cases(kinds=None, contexts=None) -> List[Case]:
    out: List[Case] = []
    for kind in KINDS:
        if kinds and kind.name not in kinds:
            continue
        ops = ops_for(kind)
        for context in CONTEXTS:
            if contexts and context not in contexts:
                continue
            if context == "gen":
                pairs = [(Op(a, "", "", None, None), Op(b, "", "", None, None))
                         for a in ("pass", "lend") for b in ("pass", "lend")]
            elif context in ("expr", "call"):
                pairs = [(a, b) for a in ops for b in ops]
            else:
                pairs = [(a, b) for a in ops for b in ops + [None]]
            for op1, op2 in pairs:
                src = program(kind, context, op1, op2)
                if src is None:
                    continue
                verdict, reasons = expect(kind, context, op1, op2)
                name = f"{kind.name}.{context}.{op1.name}.{op2.name if op2 else 'none'}"
                out.append(Case(name, kind.name, context, op1.name,
                                op2.name if op2 else "none", src, verdict,
                                list(reasons)))
    return out

# SOP Language Guide

## Purpose

SOP extends TOP with ownership-aware sequence programming. This guide introduces the language through short examples.

The goal is to write programs that are both:

1. Expressive (`Seq(T)`, loops, higher-order helpers, slicing).
2. Sound (no unsafe aliasing, no use-after-move, no escaping borrowed views).

## 1. Quick Language Snapshot

SOP adds these major capabilities:

1. **Capture-free lambdas**: anonymous functions with no outer-variable capture.
2. **Owned sequences**: `Seq(T)` values are owning and move-sensitive.
3. **Borrowed iteration**: iterate with `for (e : &s)`.
4. **Indexing and length**: `s[i]`, `#s`.
5. **Subsequences and views**: `s[i:j]` copies; `&s[i:j]` borrows a zero-copy view.
6. **Library helpers**: `fold`, `map`.

SOP also includes ADTs, pattern matching, and destructuring.

## 2. Capture-Free Lambdas

SOP adds anonymous functions using this syntax:

```top
(x) => x * 2
(acc, x) => acc + x
```

In SOP, lambdas are first-class values. You can assign them to variables and call them through those variables.

Example 1 (assign and call):

```top
main() {
  var twice, plus;

  twice = (x) => x * 2;
  plus = (a, b) => a + b;

  print(twice(21));   // 42
  print(plus(7, 5));  // 12
}
```

Example 2 (pass function-valued variables to helpers):

```top
main() {
  var nums, doubler, add, doubled, total;

  nums = [1, 2, 3, 4];
  doubler = (x) => x * 2;
  add = (acc, x) => acc + x;

  doubled = map(&nums, doubler);
  total = fold(&doubled, 0, add);

  print(total);       // 20
}
```

These lambdas are **capture-free**. They cannot reference local variables from surrounding scope.

In other words, lambdas are great for local one-off behavior, but they do not carry outside state.

## 3. Owned Sequences

Create a sequence with bracket literals:

```top
main() {
  var nums;

  nums = [1, 2, 3, 4, 5];
  print(nums);         // [1, 2, 3, 4, 5]
}
```

Use length and indexing:

```top
main() {
  var nums;

  nums = [1, 2, 3, 4, 5];
  print(#nums);      // 5
  print(nums[0]);    // 1
}
```

### Ownership intuition

`Seq(T)` is an owning value. Assigning/passing it by value can move ownership.

```top
main() {
  var a, b;

  a = [10, 20, 30];
  b = a;             // move
  print(a[0]);       // rejected: use after move
}
```

## 4. Borrowed Iteration

Iterate sequences by borrowing the sequence, not by consuming it:

```top
main() {
  var a, e, total;
  a = [1, 2, 3, 4];
  total = 0;

  for (e : &a) {
    total = total + e;
  }

  print(total);      // 10
  print(a[0]);       // still valid; a was borrowed, not moved
}
```

Iterating directly over an owned sequence is rejected:

```top
main() {
  var s, e;
  s = [1, 2, 3];
  for (e : s) {        // rejected: expected &s
    print(e);
  }
}
```

## 5. Range Loops

SOP supports range-based loops:

```top
main() {
  var i, sum;
  sum = 0;

  for (i : 1 .. 6) {
    sum = sum + i;
  }

  print(sum);           // 15
}
```

With explicit step:

```top
main() {
  var i;
  for (i : 0 .. 10 by 2) {
    print(i);           // 0,2,4,6,8
  }
}
```

## 6. Higher-Order Sequence Helpers

SOP includes `fold` and `map` as library-level functions.

### `fold`

```top
main() {
  var nums, result;
  nums = [1, 2, 3, 4, 5];

  result = fold(&nums, 0, (acc, x) => acc + x);

  print(result);        // 15
}
```

### `map`

```top
main() {
  var nums, doubled;
  nums = [1, 2, 3];

  doubled = map(&nums, (x) => x * 2);

  print(doubled);       // [2, 4, 6]
}
```

## 7. Slices and Views

SOP has two slice forms with distinct ownership semantics, distinguished by the `&` prefix — consistent with the borrow syntax used elsewhere in the language.

### Copying subsequence: `s[i:j]`

Unadorned slice syntax produces a new owned `Seq(T)` by copying elements from index `i` up to (but not including) `j`. Because `mid` is an independent copy, updating it has no effect on `s`.

```top
main() {
  var s, mid, doubled;

  s = [10, 20, 30, 40, 50];

  mid = s[1:4];                      // [20, 30, 40] — independent owned copy

  doubled = map(&mid, (x) => x * 2); // update the copy
  print(doubled);                    // [40, 60, 80]
  print(mid);                        // [20, 30, 40] — mid unchanged
  print(s);                          // [10, 20, 30, 40, 50] — s unchanged
}
```

### Borrowed view: `&s[i:j]`

Prefixing with `&` produces a zero-copy borrowed view (`Slice(T)`) directly into the memory of `s`. No allocation occurs. This is the same `&` borrow idea used in `for (e : &s)` and `fold(&s, ...)` — it means "use without taking ownership."

Because a view is read-only, you cannot update elements through it. Any computation over a view produces a new sequence; the original data is untouched:

```top
main() {
  var s, window, result;

  s = [10, 20, 30, 40, 50];

  window = &s[1:4];                    // zero-copy view of [20, 30, 40]
  result = map(&window, (x) => x + 1); // produces new Seq — does not modify s
  print(result);                       // [21, 31, 41]
  print(window);                       // [20, 30, 40] — view is unchanged
  print(s);                            // [10, 20, 30, 40, 50] — s is unchanged
}
```

A borrowed view cannot outlive the sequence it views. Returning a view of a local sequence is rejected:

```top
danger() {
  var local;
  local = [1, 2, 3];
  return &local[0:2];  // rejected: view escapes owner
}

main() {
  print(danger());
}
```

A function can safely return a borrowed view when the view is derived from a borrowed input parameter — the view's source is the caller's data, not a local:

```top
firstHalf(&s) {
  return &s[0:#s/2];   // ok: s is borrowed from the caller
}

main() {
  var nums, half;

  nums = [2, 4, 6, 8, 10];
  half = firstHalf(&nums);
  print(half);         // [2, 4, 6]
}
```

### Read-only borrows 

All borrows in SOP — `&s`, `&s[i:j]`, borrowed parameters — are **read-only**. There is no mutable borrow (sometimes called `&mut` in other languages). You cannot update elements of a sequence through a borrow or a view. Any transformation produces a new sequence rather than modifying the original in place.

If you need to update a sequence, assign a new value to the variable:

```top
main() {
  var nums;

  nums = [1, 2, 3, 4, 5];
  nums = map(&nums, (x) => x + 1);  // reassign to updated copy
  print(nums);                       // [2, 3, 4, 5, 6]
}
```

## 8. Copy vs Own Element Behavior

Element access and iteration behavior depends on the ownership class of the element type `T`.

### Copy elements

Primitive types like `int` are Copy. Indexing or iterating yields the value directly — you get a plain usable copy each time.

```top
main() {
  var nums, e, total;

  nums = [10, 20, 30];

  total = 0;
  for (e : &nums) {
    total = total + e;   // e is a plain int value
  }

  print(nums[1]);        // 20 — direct value, not a borrow
  print(total);          // 60
}
```

### Own elements

When `T` is itself an owning type — for example `Seq(int)` — elements cannot be freely copied out. Indexing yields a borrowed reference to the element inside the sequence, not a moved-out value. The sequence retains ownership.

```top
main() {
  var matrix, row;

  matrix = [[1, 2, 3], [4, 5, 6], [7, 8, 9]];

  row = &matrix[1];      // borrow row 1 — matrix retains ownership
  print(row);            // [4, 5, 6]
  print(#row);           // 3

  for (row : &matrix) {
    print(fold(&row, 0, (acc, x) => acc + x));  // sum each row
  }
                         // 6, 15, 24
}
```

Attempting to move an Own element out of a sequence is rejected:

```top
main() {
  var matrix, row;

  matrix = [[1, 2], [3, 4]];
  row = matrix[0];       // rejected: would move element out of owned sequence
  print(row);
}
```

### Ownership categories at a glance

Every SOP type falls into one of three categories:

| Category | Types | Assignment | Destructor | Returnable | Indexable |
|---|---|---|---|---|---|
| **Copy** | `int`; ADT with all-Copy fields | copies value | no | yes | — |
| **Own** | `Seq(T)`; ADT with any Own field | moves ownership | yes | yes | yes |
| **Borrowed** | `Slice(T)`, `&T` | copies descriptor | no | only if derived from a borrow parameter | yes (read-only) |

The ownership class of `Seq(T)` and ADT types is determined by their contents: a `Seq(int)` is Own, and an ADT whose every variant holds only `int` fields is Copy. An ADT with a `Seq(T)` field anywhere becomes Own.

## 9. Common Rejected Patterns

### 1) Use-after-move

```top
main() {
  var s, t;
  s = [1, 2, 3];
  t = s;               // move
  print(#s);           // rejected
}
```

### 2) Invalid iteration form

```top
main() {
  var s, e;
  s = [1, 2, 3];
  for (e : s) {       // rejected; expected &s
    print(e);
  }
}
```

### 3) Capturing lambda

```top
main() {
  var k, nums;
  k = 10;
  nums = [1, 2, 3];
  print(map(&nums, (x) => x + k)); // rejected: captures k
}
```

## 10. Mini End-to-End Example

```top
main() {
  var nums, firstHalf, doubled, total, e;

  nums = [-1, 2, -3, 4, 5, 6];

  firstHalf = nums[0:#nums/2];
  doubled = map(&firstHalf, (x) => x * 2);
  total = 0;

  for (e : &doubled) {
    if (e > 0) {
      total = total + e;
    }
  }

  print(total);
}
```

## 11. What To Practice

To get comfortable with SOP quickly, practice in this order:

1. Write small programs that build and traverse `Seq(T)` values.
2. Use `for (e : &s)` consistently until borrowed iteration feels natural.
3. Use `map` and `fold` with small capture-free lambdas.
4. Try slice expressions (`s[i:j]`, `s[i:]`, `s[:j]`) and inspect outputs.
5. Intentionally trigger rejected cases (use-after-move, invalid iteration, capturing lambdas) and read diagnostics.

If you can explain why each rejected example is unsafe and how to rewrite it safely, you are using SOP the intended way.

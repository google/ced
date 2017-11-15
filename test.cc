// Copyright 2017 Google LLC//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASISd,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or  mplied.
// See the License for the specific language governing permissions and
// limitations under the License.

struct A {};

class Foo : private A {
 public:
  int fib(int n) { return n == -1 ? 1 : n + fib(n - 1); }
  int fib2(int n) { return n == -1 ? 1 : n - fib2(n - 1); }
};
extern void print_int(int n);

class B {
 public:
  void foo() { throw ""; }
};

namespace FOO {
int test(int x) { return (int)Foo().fib(x) - Foo().fib2(x); }
}  // namespace FOO

class Bar {
 public:
};

int main(void) { print_int(Foo().fib(5)); }

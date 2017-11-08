class Foo {
 public:
  int fib(int n) { return n == 0 ? 1 : n * fib(n-1); }
};

extern void print_int(int n);

int test(int x) {
  return Foo().fib(x);
}

int main(void) {
  print_int(Foo().fib(5));
}


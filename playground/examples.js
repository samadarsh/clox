// Example programs for the dropdown. Each one is checked by
// playground/test/test_playground.py, which runs it through the real page.
const EXAMPLES = [
  {
    id: "hello",
    name: "Hello, world",
    source: `print "Hello, world!";

// Strings join with +, and numbers print without trailing zeros.
var name = "Lox";
print "Hello, " + name + "!";
print 1 + 2 * 3;
`,
  },
  {
    id: "fib",
    name: "Fibonacci, with timing",
    source: `// Recursive Fibonacci: about 2.7 million function calls.
fun fib(n) {
  if (n < 2) return n;
  return fib(n - 2) + fib(n - 1);
}

var start = clock();
print fib(30);
print "seconds:";
print clock() - start;
`,
  },
  {
    id: "closures",
    name: "Closures: a counter",
    source: `// makeCounter returns a function that remembers "count" after
// makeCounter itself has returned: a closure.
fun makeCounter() {
  var count = 0;
  fun increment() {
    count = count + 1;
    return count;
  }
  return increment;
}

var a = makeCounter();
var b = makeCounter();
print a(); // 1
print a(); // 2
print a(); // 3
print b(); // 1: b has its own count
`,
  },
  {
    id: "classes",
    name: "Classes and inheritance",
    source: `class Animal {
  init(name) {
    this.name = name;
  }

  speak() {
    return this.name + " makes a sound.";
  }
}

class Dog < Animal {
  speak() {
    return super.speak() + " Woof!";
  }
}

class Puppy < Dog {
  init(name) {
    super.init(name + " Jr.");
  }

  speak() {
    return super.speak() + " (squeaky)";
  }
}

print Animal("Cat").speak();
print Dog("Rex").speak();
print Puppy("Rex").speak();

// A method taken out of an instance remembers its "this".
var speak = Dog("Fido").speak;
print speak();
`,
  },
  {
    id: "fizzbuzz",
    name: "FizzBuzz",
    source: `// Lox has no % operator, so compute the remainder by hand.
fun mod(a, b) {
  while (a >= b) a = a - b;
  return a;
}

for (var i = 1; i <= 15; i = i + 1) {
  if (mod(i, 15) == 0) {
    print "FizzBuzz";
  } else if (mod(i, 3) == 0) {
    print "Fizz";
  } else if (mod(i, 5) == 0) {
    print "Buzz";
  } else {
    print i;
  }
}
`,
  },
  {
    id: "errors",
    name: "Errors (runtime, with a stack trace)",
    source: `fun inner(x) {
  return x + "one";   // a number plus a string: runtime error
}

fun outer() {
  return inner(1);
}

print "before the error";
outer();
print "never printed";
`,
  },
  {
    id: "forever",
    name: "Infinite loop (press Stop)",
    source: `// This never ends. The interpreter runs in a Web Worker, so the
// page stays responsive: press Stop to end it.
var i = 0;
while (true) {
  i = i + 1;
  if (i == 10000000) {
    print "still running...";
    i = 0;
  }
}
`,
  },
];

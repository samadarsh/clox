# Demo recording for the README, made with charmbracelet/vhs.
#
#   brew install vhs     (also installs ttyd and ffmpeg)
#   vhs demo.tape        (writes demo.gif; takes about a minute)
#
# The programs it runs are in demo/.

Output demo.gif
Require make
Require clang

Set Shell "bash"
Set FontSize 18
Set Width 1200
Set Height 720
Set Padding 24
Set TypingSpeed 40ms
Set Theme "Catppuccin Mocha"

# Setup, not recorded: a release build and a short prompt.
Hide
Type "make build >/dev/null && export PS1='$ ' && clear"
Enter
Sleep 4s
Show

# 1. The REPL
Type "# The REPL: one line at a time"
Enter
Type "./build/release/clox"
Enter
Sleep 500ms
Type 'var greeting = "Hello";'
Enter
Sleep 300ms
Type 'print greeting + ", Lox!";'
Enter
Sleep 700ms
Type "fun square(x) { return x * x; }"
Enter
Sleep 300ms
Type "print square(12);"
Enter
Sleep 1s
Ctrl+D
Sleep 1.5s

Hide
Type "clear"
Enter
Show

# 2. Classes, inheritance and closures
Type "# Classes, inheritance and closures"
Enter
Type "cat demo/zoo.lox"
Enter
Sleep 2.5s
Type "./build/release/clox demo/zoo.lox"
Enter
Sleep 3s

Hide
Type "clear"
Enter
Show

# 3. A runtime error with a stack trace
Type "# A runtime error prints a stack trace (exit code 70)"
Enter
Type "cat demo/oops.lox"
Enter
Sleep 2s
Type `./build/release/clox demo/oops.lox; echo "exit code $?"`
Enter
Sleep 3s

Hide
Type "clear"
Enter
Show

# 4. Speed
Type "# fib(30): about 2.7 million function calls"
Enter
Type "./build/release/clox demo/fib.lox"
Enter
Sleep 3s

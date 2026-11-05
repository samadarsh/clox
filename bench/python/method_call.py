# Python version of bench/lox/method_call.lox: method calls, including
# super calls and fields read and written through `self`.
import time


def lox(value):
    # Print booleans the way Lox does, so the outputs can be compared.
    return "true" if value else "false"


class Toggle:
    def __init__(self, start_state):
        self.state = start_state

    def value(self):
        return self.state

    def activate(self):
        self.state = not self.state
        return self


class NthToggle(Toggle):
    def __init__(self, start_state, max_counter):
        super().__init__(start_state)
        self.count_max = max_counter
        self.count = 0

    def activate(self):
        self.count = self.count + 1
        if self.count >= self.count_max:
            super().activate()
            self.count = 0
        return self


start = time.process_time()
n = 100000
val = True
toggle = Toggle(val)

for i in range(n):
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()

print(lox(toggle.value()))

val = True
ntoggle = NthToggle(val, 3)

for i in range(n):
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()

print(lox(ntoggle.value()))
print(time.process_time() - start)

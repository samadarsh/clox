# Python version of bench/lox/equality.lox: a loop-heavy program. The first
# loop evaluates and discards literals, the second compares values of mixed
# types with ==. Ten million iterations each.
import time

i = 0

loop_start = time.process_time()

while i < 10000000:
    i = i + 1

    1; 1; 1; 2; 1; None; 1; "str"; 1; True
    None; None; None; 1; None; "str"; None; True
    True; True; True; 1; True; False; True; "str"; True; None
    "str"; "str"; "str"; "stru"; "str"; 1; "str"; None; "str"; True

loop_time = time.process_time() - loop_start

start = time.process_time()

i = 0
while i < 10000000:
    i = i + 1

    1 == 1; 1 == 2; 1 == None; 1 == "str"; 1 == True
    None == None; None == 1; None == "str"; None == True
    True == True; True == 1; True == False; True == "str"; True == None
    "str" == "str"; "str" == "stru"; "str" == 1; "str" == None; "str" == True

elapsed = time.process_time() - start
print("loop")
print(loop_time)
print("elapsed")
print(elapsed)
print("equals")
print(elapsed - loop_time)

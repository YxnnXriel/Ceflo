src=$(find $ceflo/src -name "*.c")
include=$(find $ceflo/src/include -name "*.h")

gcc -Wall -Wextra -fPIC -shared -o $ceflo/lib/math/libmath.so $ceflo/lib/math/math.c $src 


src=$(find $ceflo/src -name "*.c")
# src=$(find $ceflo/src -name "*.c" | grep -E "ast.c|visitor.c")

include=$(find $ceflo/src/include -name "*.h")

# gcc -Wall -Wextra -fPIC -shared -o $ceflo/lib/math/libmath.so $ceflo/lib/math/math.c $src 
gcc -Wall -Wextra -fPIC -shared -o $ceflo/lib/file/libfile.so -I $ceflo/src/include $ceflo/lib/file/file.c $src 

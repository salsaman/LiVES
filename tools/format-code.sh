find . \( \( -path ./lives-plugins/weed-plugins -o -path ./intl -o -path ./src/back\* -o -path ./libweed \
     -o  -name lsd.h \) -prune \) \
     -o \( -name \*.c -o -name \*.h -o -name \*.cpp -o -name \*.hpp \) -print \
    | xargs astyle --style=java -H -Y -s2 -U -k3 -W3 -xC128 -xL -p -o -Q -O -xp
git diff --check
#astyle --style=java -H -Y -U -k3 -W3 -xC140 -xL -p -U -k3 -xw smogrify

find . -name \*.c -or -name \*.h -or -name \*.cpp -or -name \*.hpp | xargs astyle --style=java -H -Y -s2 -U -k3 -W3 -xC140 -xL
astyle --style=java -H -Y -s4 -U -k3 -W3 -xC140 -xL smogrify

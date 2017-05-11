find . -name \*.c -or -name \*.h -or -name \*.cpp -or -name \*.hpp -or -name smogrify | xargs astyle --style=java -H -Y -s2 -U -k3 -W3 -xC140 -xL -p -U -k3

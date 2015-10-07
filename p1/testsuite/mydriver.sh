#!/usr/bin/env bash
./sdriver.pl -t $1 -s ../tsh > tsh-my-output.txt
./sdriver.pl -t $1 -s ./tsh-orig.32 > tsh-ref-output.txt
diff -u tsh-my-output.txt tsh-ref-output.txt

#!/bin/sh

# Question 10 timing script.
# Build first, make one large file, then time all three compression versions.

DATA_FILE="data/question10-large-input.txt"
SEQ_OUT="data/question10-sequential-output.txt"
FORK_OUT="data/question10-fork-output.txt"
THREAD_OUT="data/question10-thread-output.txt"

mkdir -p data
rm -f "$DATA_FILE" "$SEQ_OUT" "$FORK_OUT" "$THREAD_OUT"

# Long runs make the compression work large enough to time.
printf "Creating large test file...\n"
i=0
while [ "$i" -lt 50000 ]; do
    printf "00000000000000000000000000000000111111111111111111111111111111111\n" >> "$DATA_FILE"
    i=$((i + 1))
done

printf "\nTiming sequential version: MyCompress\n"
/usr/bin/time -p ./bin/MyCompress "$DATA_FILE" "$SEQ_OUT"

printf "\nTiming forked process version: ParFork\n"
/usr/bin/time -p ./bin/ParFork "$DATA_FILE" "$FORK_OUT"

printf "\nTiming pthread version: ParThread\n"
/usr/bin/time -p ./bin/ParThread "$DATA_FILE" "$THREAD_OUT" 4

printf "\nOutput files created:\n"
ls -lh "$SEQ_OUT" "$FORK_OUT" "$THREAD_OUT"

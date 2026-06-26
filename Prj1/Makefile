CC = gcc
CFLAGS = -Wall -Wextra -std=c11
PTHREAD_FLAGS = -pthread
BIN_DIR = bin

TARGETS = \
	$(BIN_DIR)/MyCompress \
	$(BIN_DIR)/MyDecompress \
	$(BIN_DIR)/ForkCompress \
	$(BIN_DIR)/PipeCompress \
	$(BIN_DIR)/ParFork \
	$(BIN_DIR)/MiniShell \
	$(BIN_DIR)/MoreShell \
	$(BIN_DIR)/DupShell \
	$(BIN_DIR)/ParThread

.PHONY: all clean

all: $(TARGETS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/MyCompress: questions-1-10/01-system-calls-mycompress/MyCompress.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/MyDecompress: questions-1-10/02-decompress-mydecompress/MyDecompress.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/ForkCompress: questions-1-10/03-fork-compress/ForkCompress.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/PipeCompress: questions-1-10/04-pipe-compress/PipeCompress.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/ParFork: ParFork.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/MiniShell: MiniShell.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/MoreShell: MoreShell.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/DupShell: questions-1-10/08-dup-shell/DupShell.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/ParThread: questions-1-10/09-parallel-thread/ParThread.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $(PTHREAD_FLAGS) $< -o $@

clean:
	rm -rf $(BIN_DIR)

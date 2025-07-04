CC = gcc
CFLAGS = -Wall -Wextra -g -I.
LDFLAGS = -lreadline
TARGET = ash

TESTS = tests/test_vars tests/test_tokenizer tests/test_parser

all: $(TARGET)

$(TARGET): shell.c vars.c parser.c tokenizer.c
	$(CC) $(CFLAGS) -o $(TARGET) shell.c vars.c parser.c tokenizer.c $(LDFLAGS)

# ---------------- Tests ----------------
tests/test_vars: tests/test_vars.c vars.c vars.h
	$(CC) $(CFLAGS) -o $@ tests/test_vars.c vars.c

tests/test_tokenizer: tests/test_tokenizer.c tokenizer.c tokenizer.h
	$(CC) $(CFLAGS) -o $@ tests/test_tokenizer.c tokenizer.c

tests/test_parser: tests/test_parser.c parser.c vars.c tokenizer.c
	$(CC) $(CFLAGS) -o $@ tests/test_parser.c parser.c vars.c tokenizer.c

test: $(TESTS)
	@for t in $(TESTS); do \
		echo "Running $$t"; \
		./$$t || exit 1; \
	done; \
	echo "All unit tests passed"

clean:
	rm -f $(TARGET) *.o $(TESTS)

.PHONY: all clean test

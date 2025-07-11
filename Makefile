CC = gcc
SRCDIR := src
INCDIR := include
SRC := $(wildcard $(SRCDIR)/*.c)
OBJ := $(SRC:.c=.o)
TARGET := ash

CFLAGS := -Wall -Wextra -g -I$(INCDIR)
LDFLAGS := -lreadline

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET) $(TESTS)

# ---------------- Tests ----------------
TESTS := \
  tests/test_vars \
  tests/test_tokenizer \
  tests/test_parser \
  tests/test_tokenizer_quotes \
  tests/test_case \
  tests/test_glob \
  tests/test_alias \
  tests/test_heredoc \
  tests/test_completion \
  tests/test_syntax

tests/test_vars: tests/test_vars.c src/vars.c
	$(CC) $(CFLAGS) $^ src/arith.c -o $@

tests/test_tokenizer: tests/test_tokenizer.c src/tokenizer.c
	$(CC) $(CFLAGS) $^ -o $@

tests/test_parser: tests/test_parser.c src/parser.c src/tokenizer.c src/vars.c src/arith.c
	$(CC) $(CFLAGS) $^ -o $@

tests/test_tokenizer_quotes: tests/test_tokenizer_quotes.c src/tokenizer.c
	$(CC) $(CFLAGS) $^ -o $@

tests/test_case: tests/test_case.c src/parser.c src/tokenizer.c src/vars.c src/arith.c
	$(CC) $(CFLAGS) $^ -o $@

tests/test_glob: tests/test_glob.c src/globbing.c src/tokenizer.c
	$(CC) $(CFLAGS) $^ -o $@

tests/test_alias: tests/test_alias.c src/alias.c src/tokenizer.c
	$(CC) $(CFLAGS) $^ -o $@

tests/test_heredoc: tests/test_heredoc.c ash
	$(CC) $(CFLAGS) $< -o $@

tests/test_completion: tests/test_completion.c src/completion.c
	$(CC) $(CFLAGS) $^ -o $@

tests/test_syntax: tests/test_syntax.c src/syntax.c
	$(CC) $(CFLAGS) $^ -o $@

test: $(TESTS)
	@for t in $(TESTS); do \
	  echo "Running $$t"; \
	  ./$$t || exit 1; \
	done; \
	echo "All unit tests passed"

.PHONY: all clean test

# ASH - Unix Shell

A Unix shell implementation inspired by bash and sh, built to understand how shells work internally.

## Features

- Command execution with arguments
- Built-in commands: `cd`, `exit`, `history`, `alias`, `unalias`
- Background processes with `&`
- I/O redirection (`>`, `>>`, `<`)
- Pipes (`|`) with arbitrary length pipelines
- Job control (Ctrl+Z, `jobs`, `fg`, `bg`)
- Command history
- Command substitution (`$(command)` and backtick syntax)
- Wildcard expansion (`*.c`, `file?.txt`, `[abc]*`)
- Command aliases

## Project Structure

```
ash/
├── src/               # C source files
│   ├── shell.c        # Main shell logic
│   ├── terminal.c     # Terminal handling
│   ├── jobs.c         # Job management
│   ├── io.c           # I/O redirection
│   ├── builtins.c     # Built-in commands
│   ├── history.c      # Command history
│   ├── parser.c       # Command parsing
│   ├── tokenizer.c    # Tokenization
│   ├── vars.c         # Environment variables
│   └── arith.c        # Arithmetic expressions
├── include/           # Header files
├── tests/             # Test suite
└── Makefile           # Build configuration
```

## Building

**Requirements:**
- GCC
- readline library (`sudo apt install libreadline-dev`)
- Make

**Build:**
```bash
make
```

## Usage

**Interactive mode:**
```bash
./ash
```

**Run script:**
```bash
./ash script.ash arg1 arg2
```

**Single command:**
```bash
./ash -c 'echo hello world'
```

## Examples

**Job control:**
```bash
# Background process
ash> sleep 10 &
[1] 12345

# List jobs
ash> jobs
[1] 12345 Running    sleep 10

# Bring to foreground
ash> fg 1
```

**Pipes and redirection:**
```bash
# Pipeline
ash> ls | grep .txt | sort

# Redirection
ash> ls > files.txt
ash> echo "more" >> files.txt
ash> sort < files.txt
```

**Command substitution:**
```bash
ash> echo "Today is $(date)"
ash> echo "Files: `ls | wc -l`"
```

**Basic scripting:**
```bash
# Loops and conditionals
count=0
while [ "$count" -lt 3 ]; do
    echo "Count: $count"
    count=$(expr $count + 1)
done

if ls *.c >/dev/null 2>&1; then
    echo "Found C files"
fi
```

## Testing

```bash
make test    # Run tests
make clean   # Clean build files
```

## License

MIT License

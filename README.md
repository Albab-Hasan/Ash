# ASH - My Unix-like Shell Project

This is a shell I've been building to learn more about how Unix shells work under the hood. It's inspired by bash and sh, but much simpler.

Note: I know the commit history is all over the place, and I'm really sorry about that. This project came together over a long period of time, mostly as a side project, and I didn’t even start using Git until I was already pretty far into it. I’m still not very experienced with Git (as you can probably tell from my profile), and I used to forget to commit changes regularly.

A lot of the early commits are messy — sometimes I’d fix multiple things but only mention one in the message, or I’d just write something vague or unhelpful. It’s definitely not ideal, and I get that it might be frustrating to go through.

That said, things improve later on. Around commit #18 I cleaned up the project structure, and by commit #21 I started being more consistent with commits and writing proper messages.

Thanks for understanding, and again, sorry if the earlier commits are a pain to navigate.

## What's in the box

The shell supports basic features like:
- Running commands with arguments
- Built-ins like `cd`, `exit`, `history`
- Background processes with `&`
- I/O redirection (`>`, `>>`, `<`)
- Pipes (`|`)
- Job control (Ctrl+Z, `jobs`, `fg`)
- Command history and tab completion
- Command substitution (`$(command)` and backtick syntax)

## Project Structure

```
ash/
├── src/               # C source files
│   ├── shell.c        # Main shell code
│   ├── terminal.c     # Terminal handling
│   ├── jobs.c         # Background job management
│   ├── io.c           # Redirection stuff
│   ├── builtins.c     # Built-in commands
│   ├── history.c      # Command history
│   ├── parser.c       # Script parsing
│   ├── tokenizer.c    # Command tokenizing
│   ├── vars.c         # Environment variables
│   └── arith.c        # Math expressions
├── include/           # Header files
├── tests/             # Some basic tests
└── Makefile           # Build configuration
```

## Building & Running

### Requirements

- GCC
- readline library (`sudo apt install libreadline-dev` on Ubuntu/Debian)
- Make

### Building

Just run:
```bash
make
```

### Running

Interactive mode:
```bash
./ash
```

Run a script:
```bash
./ash script.ash arg1 arg2
```

Run a single command:
```bash
./ash -c 'echo hello world'
```

## Cool Features

### Job Control

The shell can handle background jobs properly:

```bash
# Start a job in background
ash> sleep 10 &
[1] 12345

# List running jobs
ash> jobs
[1] 12345 Running    sleep 10

# Bring to foreground
ash> fg 1
```

You can also suspend jobs with Ctrl+Z and resume them later with `fg` or `bg`.

### Pipes and Redirection

```bash
# Pipe output between commands
ash> ls | grep .txt

# Save output to file
ash> ls > files.txt

# Append to file
ash> echo "more stuff" >> files.txt

# Read from file
ash> sort < files.txt
```

### Command Substitution

You can use the output of a command as part of another command:

```bash
# Modern syntax
ash> echo "Today is $(date)"

# Traditional backtick syntax
ash> echo "Files: `ls | wc -l`"

# Nested substitutions
ash> echo "The date is $(echo `date`)"
```

### Scripting

I've added basic scripting support. Here's a simple example:

```sh
# Count to 3
count=0
while [ "$count" -lt 3 ]; do
    echo "Count: $count"
    count=$(expr $count + 1)
done

# Check for C files
if ls *.c >/dev/null 2>&1; then
    echo "Found some C files!"
else
    echo "No C files here."
fi
```

## Challenges I Faced

Building this shell taught me a lot about:

1. **Process management** - Creating, tracking, and controlling child processes
2. **Signal handling** - Dealing with Ctrl+C, Ctrl+Z, etc.
3. **Terminal control** - Managing which process group controls the terminal
4. **File descriptors** - Handling pipes and redirections properly

The trickiest part was getting job control working right - making sure background processes don't try to read from the terminal and that foreground processes receive signals properly.

## What's Next

I'm still working on:
- Better support for multi-stage pipelines
- Wildcard expansion
- Heredocs (`<<EOF`)
- Aliases

## Building & Testing

```bash
# Regular build
make

# Run tests
make test

# Clean up
make clean
```

## License

This project is under the MIT License (see `LICENSE`).

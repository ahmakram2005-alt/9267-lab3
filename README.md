# Lab 3 - Mini Shell

A simple shell written in C++.

## Run

```bash
make
./myshell
```

## Features

- Run commands with arguments.
- Support pipes and file redirection.
- Run background commands using &.
- Support cd and exit.
- Keep the shell running after Ctrl+C.
- Save finished-child messages in process.log.

## Examples

```bash
ls -al
echo "Hello" | grep "Hello"
echo hello > output.txt
cat < output.txt
sleep 3 &
exit
```

## Notes

The wildcard bonus is not included.
Use cd and exit alone, without pipes or redirection.
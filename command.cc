/*
 * 
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include <cerrno>
#include "command.h"
#include "tokenizer.h"

static int logFile = -1;

// Record a finished child.
static void logChild()
{
    const char message[] = "Child terminated\n";
    if (logFile >= 0) write(logFile, message, sizeof(message) - 1);
}

static void reapChildren(int)
{
    int saved = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) logChild();
    errno = saved;
}

// Close opened file descriptors.
static void closeFiles(int a, int b, int c)
{
    if (a >= 0) close(a);
    if (b >= 0) close(b);
    if (c >= 0) close(c);
}

// Connect a file or pipe to input, output or error.
static void connectFile(int source, int target)
{
    if (source >= 0 && dup2(source, target) < 0) {
        perror("dup2");
        _exit(1);
    }
}

// Save a filename and reject repeated redirection.
static bool setFile(Command &command, TokenType type, const char *name)
{
    switch (type) {
        case TOKEN_INPUT:
            if (command._inputFile) return false;
            command._inputFile = strdup(name);
            return true;
        case TOKEN_ERROR:
            if (command._errFile) return false;
            command._errFile = strdup(name);
            return true;
        case TOKEN_REDIRECT:
        case TOKEN_APPEND:
        case TOKEN_REDIRECT_AND_ERROR:
            if (command._outFile) return false;
            if (type == TOKEN_REDIRECT_AND_ERROR && command._errFile) return false;
            command._outFile = strdup(name);
            command._append = type != TOKEN_REDIRECT;
            if (type == TOKEN_REDIRECT_AND_ERROR) {
                command._errFile = strdup(name);
                command._out_error = 1;
            }
            return true;
        default:
            return false;
    }
}

void parse(std::vector<Token> &tokens)
{
    // Join quoted words split by the original tokenizer.
    std::vector<Token> words;
    std::string word;
    char quote = 0;
    bool quoted = false;
    for (Token token : tokens) {
        if (token.type == TOKEN_EOF) break;
        if (quote) word += " ";
        for (char c : token.value) {
            if (quote) {
                if (c == quote) quote = 0;
                else word += c;
            } else if (c == '\'' || c == '"') {
                quote = c;
                quoted = true;
            } else word += c;
        }
        if (!quote) {
            if (quoted) token.type = TOKEN_ARGUMENT;
            words.push_back({token.type, word});
            word.clear();
            quoted = false;
        }
    }
    if (quote) {
        fprintf(stderr, "Missing closing quote\n");
        return;
    }
    if (words.empty()) return;

    Command command;
    SimpleCommand *simple = new SimpleCommand();
    command.insertSimpleCommand(simple);
    bool valid = true;
    for (size_t i = 0; i < words.size() && valid; i++) {
        Token token = words[i];
        switch (token.type) {
            case TOKEN_COMMAND:
            case TOKEN_ARGUMENT:
                simple->insertArgument(strdup(token.value.c_str()));
                break;
            case TOKEN_PIPE:
                if (!simple->_numberOfArguments) valid = false;
                else {
                    simple = new SimpleCommand();
                    command.insertSimpleCommand(simple);
                }
                break;
            case TOKEN_BACKGROUND:
                valid = i + 1 == words.size();
                command._background = 1;
                break;
            case TOKEN_INPUT:
            case TOKEN_ERROR:
            case TOKEN_REDIRECT:
            case TOKEN_APPEND:
            case TOKEN_REDIRECT_AND_ERROR:
                if (i + 1 == words.size() ||
                    (words[i + 1].type != TOKEN_COMMAND &&
                     words[i + 1].type != TOKEN_ARGUMENT)) valid = false;
                else valid = setFile(command, token.type, words[++i].value.c_str());
                break;
            default:
                valid = false;
        }
    }
    if (!simple->_numberOfArguments) valid = false;

    // cd and exit must run in the parent shell.
    bool builtin = false;
    for (int i = 0; i < command._numberOfSimpleCommands; i++) {
        char *name = command._simpleCommands[i]->_arguments[0];
        if (name && (!strcmp(name, "cd") || !strcmp(name, "exit"))) builtin = true;
    }
    if (builtin && (command._numberOfSimpleCommands != 1 || command._background ||
                    command._inputFile || command._outFile || command._errFile)) valid = false;
    if (!valid) fprintf(stderr, "Invalid command, pipe or redirection\n");
    else if (builtin) {
        char **args = simple->_arguments;
        if (!strcmp(args[0], "exit")) {
            printf("Good bye!!\n");
            exit(0);
        }
        const char *directory = args[1];
        if (!directory) directory = getenv("HOME");
        if (simple->_numberOfArguments > 2) fprintf(stderr, "cd: too many arguments\n");
        else if (!directory) fprintf(stderr, "cd: HOME is not set\n");
        else if (chdir(directory) < 0) perror("cd");
    } else command.execute();
    command.clear();
    free(command._simpleCommands);
}

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
    _arguments[0] = NULL;
}

void SimpleCommand::insertArgument(char *argument)
{
    if (_numberOfAvailableArguments == _numberOfArguments + 1) {
        _numberOfAvailableArguments *= 2;
        _arguments = (char **)realloc(_arguments,
            _numberOfAvailableArguments * sizeof(char *));
    }
    _arguments[_numberOfArguments] = argument;
    _arguments[_numberOfArguments + 1] = NULL;
    _numberOfArguments++;
}

Command::Command()
{
    _numberOfAvailableSimpleCommands = 1;
    _simpleCommands = (SimpleCommand **)malloc(
        _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _out_error = 0;
    _background = 0;
    _append = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands) {
        _numberOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
            _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    }
    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}

void Command::clear()
{
    for (int i = 0; i < _numberOfSimpleCommands; i++) {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
            free(_simpleCommands[i]->_arguments[j]);
        free(_simpleCommands[i]->_arguments);
        delete _simpleCommands[i];
    }
    if (_outFile) free(_outFile);
    if (_inputFile) free(_inputFile);
    if (_errFile) free(_errFile);
    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _out_error = 0;
    _background = 0;
    _append = 0;
}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
    }

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default", _out_error == 1 ? _errFile : "default"
           ,_background ? "YES" : "NO");
    printf("\n\n");
    fflush(stdout);
}

void Command::execute()
{
    print();
    int input = -1, output = -1, error = -1;
    if (_inputFile) input = open(_inputFile, O_RDONLY);
    int flags = O_WRONLY | O_CREAT | O_TRUNC;
    if (_append) flags = O_WRONLY | O_CREAT | O_APPEND;
    if (_outFile) output = open(_outFile, flags, 0666);
    if (_errFile) {
        if (_out_error) error = dup(output);
        else error = open(_errFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    }
    if ((_inputFile && input < 0) || (_outFile && output < 0) ||
        (_errFile && error < 0)) {
        perror("redirection");
        closeFiles(input, output, error);
        return;
    }
    std::vector<pid_t> children;
    int previous = input;
    for (int i = 0; i < _numberOfSimpleCommands; i++) {
        int pipeFD[2] = {-1, -1};
        bool last = i == _numberOfSimpleCommands - 1;
        if (!last && pipe(pipeFD) < 0) { perror("pipe"); break; }
        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, _background ? SIG_IGN : SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            sigset_t empty;
            sigemptyset(&empty);
            sigprocmask(SIG_SETMASK, &empty, NULL);
            connectFile(previous, STDIN_FILENO);
            int destination = output;
            if (!last) destination = pipeFD[1];
            connectFile(destination, STDOUT_FILENO);
            connectFile(error, STDERR_FILENO);
            closeFiles(previous, output, error);
            closeFiles(pipeFD[0], pipeFD[1], logFile);
            execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
            perror(_simpleCommands[i]->_arguments[0]);
            _exit(127);
        }
        if (pid < 0) {
            perror("fork");
            closeFiles(pipeFD[0], pipeFD[1], -1);
            break;
        }
        children.push_back(pid);
        if (previous >= 0) close(previous);
        previous = pipeFD[0];
        if (pipeFD[1] >= 0) close(pipeFD[1]);
    }
    closeFiles(previous, output, error);
    if (!_background) {
        for (pid_t pid : children) {
            while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {}
            logChild();
        }
    }
}

void Command::prompt()
{
    std::string input;
    while (true) {
        printf("myshell> ");
        fflush(stdout);
        if (!std::getline(std::cin, input)) { printf("\nGood bye!!\n"); break; }
        std::vector<Token> tokens = tokenize(input);
        // Protect foreground waiting from the SIGCHLD handler.
        sigset_t block, old;
        sigemptyset(&block);
        sigaddset(&block, SIGCHLD);
        sigprocmask(SIG_BLOCK, &block, &old);
        parse(tokens);
        sigprocmask(SIG_SETMASK, &old, NULL);
    }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int main()
{
    logFile = open("process.log", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0666);
    if (logFile < 0) { perror("process.log"); return 1; }
    signal(SIGINT, SIG_IGN);
    struct sigaction action = {};
    action.sa_handler = reapChildren;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &action, NULL);
    Command::_currentCommand.prompt();
    close(logFile);
    return 0;
}
#include "tokenizer.h"
#include <sstream>
#include <iostream>
#include <cctype>
#include <cstdio>

// Save one word.
static void addWord(
    std::vector<Token> &tokens,
    std::string &word,
    bool &started
)
{
    if (started)
        tokens.push_back({TOKEN_ARGUMENT, word});

    word.clear();
    started = false;
}

std::vector<Token> tokenize(const std::string &input)
{
    std::vector<Token> tokens;
    std::string word;

    char quote = 0;
    bool started = false;

    for (size_t i = 0; i < input.size(); i++)
    {
        char c = input[i];

        // Keep quoted text together.
        if (quote)
        {
            if (c == quote)
                quote = 0;
            else
                word += c;

            continue;
        }

        if (c == '\'' || c == '"')
        {
            quote = c;
            started = true;
            continue;
        }

        // A space ends the word.
        if (std::isspace((unsigned char)c))
        {
            addWord(tokens, word, started);
            continue;
        }

        // Find an operator.
        TokenType type;
        size_t length = 0;

        if (input.compare(i, 3, ">>&") == 0)
        {
            type = TOKEN_REDIRECT_AND_ERROR;
            length = 3;
        }
        else if (input.compare(i, 2, ">>") == 0)
        {
            type = TOKEN_APPEND;
            length = 2;
        }
        else if (!started && input.compare(i, 2, "2>") == 0)
        {
            type = TOKEN_ERROR;
            length = 2;
        }
        else if (c == '>')
        {
            type = TOKEN_REDIRECT;
            length = 1;
        }
        else if (c == '<')
        {
            type = TOKEN_INPUT;
            length = 1;
        }
        else if (c == '|')
        {
            type = TOKEN_PIPE;
            length = 1;
        }
        else if (c == '&')
        {
            type = TOKEN_BACKGROUND;
            length = 1;
        }

        if (length)
        {
            addWord(tokens, word, started);

            tokens.push_back({
                type,
                input.substr(i, length)
            });

            i += length - 1;
        }
        else
        {
            word += c;
            started = true;
        }
    }

    if (quote)
    {
        fprintf(stderr, "Missing closing quote\n");
        return {};
    }

    addWord(tokens, word, started);
    tokens.push_back({TOKEN_EOF, ""});

    return tokens;
}
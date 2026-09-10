#include <chrono>
#include <cctype>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

static_assert(__cplusplus > 202302L,
              "Compile this program in C++26 mode (for GCC 14: -std=c++2c).");

enum class TokenType {
    Keyword,
    Identifier,
    Operator,
    Number,
    Symbol
};

struct Token {
    TokenType type;
    std::string lexeme;
};

class LexicalAnalyzer {
public:
    [[nodiscard]] std::vector<Token> tokenize(std::string_view source) const {
        std::vector<Token> tokens;
        tokens.reserve(source.size() / 2 + 1);

        std::size_t i = 0;
        while (i < source.size()) {
            const unsigned char current = static_cast<unsigned char>(source[i]);

            if (std::isspace(current)) {
                ++i;
                continue;
            }

            if (std::isalpha(current)) {
                const std::size_t start = i++;
                while (i < source.size()) {
                    const unsigned char ch = static_cast<unsigned char>(source[i]);
                    if (!std::isalnum(ch)) {
                        break;
                    }
                    ++i;
                }

                std::string lexeme{source.substr(start, i - start)};
                const TokenType type = keywords_.contains(lexeme)
                    ? TokenType::Keyword
                    : TokenType::Identifier;
                tokens.push_back({type, std::move(lexeme)});
                continue;
            }

            if (std::isdigit(current)) {
                const std::size_t start = i++;
                while (i < source.size() &&
                       std::isdigit(static_cast<unsigned char>(source[i]))) {
                    ++i;
                }

                if (i < source.size() && source[i] == '.') {
                    if (i + 1 >= source.size() ||
                        !std::isdigit(static_cast<unsigned char>(source[i + 1]))) {
                        throw lexical_error(source, i, "decimal point must be followed by a digit");
                    }
                    ++i; // consume '.'
                    while (i < source.size() &&
                           std::isdigit(static_cast<unsigned char>(source[i]))) {
                        ++i;
                    }
                }

                tokens.push_back({TokenType::Number,
                                  std::string{source.substr(start, i - start)}});
                continue;
            }

            // Longest match: recognize == before =.
            if (source[i] == '=') {
                if (i + 1 < source.size() && source[i + 1] == '=') {
                    tokens.push_back({TokenType::Operator, "=="});
                    i += 2;
                } else {
                    tokens.push_back({TokenType::Operator, "="});
                    ++i;
                }
                continue;
            }

            if (is_single_operator(source[i])) {
                tokens.push_back({TokenType::Operator, std::string(1, source[i])});
                ++i;
                continue;
            }

            if (is_symbol(source[i])) {
                tokens.push_back({TokenType::Symbol, std::string(1, source[i])});
                ++i;
                continue;
            }

            throw lexical_error(source, i, "invalid character");
        }

        return tokens;
    }

private:
    const std::unordered_set<std::string> keywords_{"if", "else", "while", "return"};

    [[nodiscard]] static bool is_single_operator(char ch) noexcept {
        return ch == '+' || ch == '-' || ch == '*' || ch == '\\';
    }

    [[nodiscard]] static bool is_symbol(char ch) noexcept {
        return ch == '(' || ch == ')' || ch == ';' || ch == '{' || ch == '}';
    }

    [[nodiscard]] static std::runtime_error lexical_error(
        std::string_view source,
        std::size_t position,
        std::string_view message) {
        const char bad = position < source.size() ? source[position] : '?';
        return std::runtime_error(
            std::string{message} + " at position " + std::to_string(position) +
            " near '" + bad + "'");
    }
};

[[nodiscard]] constexpr std::string_view token_type_name(TokenType type) noexcept {
    switch (type) {
        case TokenType::Keyword:    return "KEYWORD";
        case TokenType::Identifier: return "IDENTIFIER";
        case TokenType::Operator:   return "OPERATOR";
        case TokenType::Number:     return "NUMBER";
        case TokenType::Symbol:     return "SYMBOL";
    }
    return "UNKNOWN";
}

void print_tokens(const std::vector<Token>& tokens) {
    for (const auto& token : tokens) {
        std::cout << '[' << token_type_name(token.type)
                  << ": " << token.lexeme << "]\n";
    }
}

void run_demo(const LexicalAnalyzer& lexer,
              std::string_view label,
              std::string_view input) {
    std::cout << "=== " << label << " ===\n";
    std::cout << "INPUT: " << input << "\n";
    try {
        print_tokens(lexer.tokenize(input));
    } catch (const std::exception& ex) {
        std::cout << "LEXICAL ERROR: " << ex.what() << '\n';
    }
    std::cout << '\n';
}

void run_benchmark(const LexicalAnalyzer& lexer) {
    std::cout << "=== PERFORMANCE (C++26, -O2) ===\n";
    std::cout << "Lines,Characters,Tokens,Time_ms\n";

    for (const int lines : {10, 100, 1000, 10000}) {
        std::string input;
        input.reserve(static_cast<std::size_t>(lines) * 42);
        for (int i = 0; i < lines; ++i) {
            input += "if (value" + std::to_string(i) + " == 3.14) return value" +
                     std::to_string(i) + " + 1; ";
        }

        // Run multiple iterations and report the best time to reduce scheduler noise.
        double best_ms = 1e100;
        std::size_t token_count = 0;
        for (int repeat = 0; repeat < 5; ++repeat) {
            const auto start = std::chrono::steady_clock::now();
            const auto tokens = lexer.tokenize(input);
            const auto end = std::chrono::steady_clock::now();
            token_count = tokens.size();
            const std::chrono::duration<double, std::milli> elapsed = end - start;
            if (elapsed.count() < best_ms) {
                best_ms = elapsed.count();
            }
        }

        std::cout << lines << ',' << input.size() << ',' << token_count << ','
                  << std::fixed << std::setprecision(3) << best_ms << '\n';
    }
}

int main() {
    const LexicalAnalyzer lexer;

    run_demo(lexer, "Test 1", "if (x == 10) return y + z;");
    run_demo(lexer, "Test 2", "while (count1 == 3.14) { count1 = count1 + 1; }");
    run_demo(lexer, "Test 3", "else return value2 \\ 5;");
    run_demo(lexer, "Test 4", "ifx = 123;");
    run_demo(lexer, "Edge case: invalid float", "x = 3.;");
    run_demo(lexer, "Edge case: invalid character", "x = @;");

    run_benchmark(lexer);
    return 0;
}

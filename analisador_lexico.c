#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

typedef enum {
    TK_KEYWORD, TK_ID, TK_INT, TK_FLOAT, TK_CHAR, 
    TK_STRING, TK_OP, TK_SPECIAL, TK_EOF, TK_UNKNOWN
} TokenType;

int line_count = 1;
int col_count = 0;

int get_char(FILE *f) {
    int c = fgetc(f);
    if (c != EOF) col_count++;
    return c;
}

void unget_char(int c, FILE *f) {
    if (c != EOF) {
        ungetc(c, f);
        col_count--;
    }
}

bool is_keyword(const char *s) {
    const char *kw[] = {
        "int", "float", "bool", "char", "if", "else", 
        "while", "for", "return", "void", "true", "false", "printf"
    };
    int n = sizeof(kw) / sizeof(kw[0]);
    for (int i = 0; i < n; i++) {
        if (strcmp(s, kw[i]) == 0) return true;
    }
    return false;
}

void handle_lexical_error(FILE *file, const char *msg, char c) {
    if (c != '\0' && !isspace(c)) {
        printf(">>> [ERRO LEXICO L%d:C%d]: %s '%c'\n", line_count, col_count, msg, c);
    } else {
        printf(">>> [ERRO LEXICO L%d:C%d]: %s\n", line_count, col_count, msg);
    }

    int next;
    while ((next = get_char(file)) != EOF) {
        if (next == '\n') {
            line_count++;
            col_count = 0;
            break;
        }
        if (next == ';' || isspace(next)) {
            if (next == ';') unget_char(next, file);
            break;
        }
    }
}

void process_file(FILE *file) {
    int c;
    while ((c = get_char(file)) != EOF) {
        
        if (isspace(c)) {
            if (c == '\n') {
                line_count++;
                col_count = 0;
            }
            continue;
        }

        int start_col = col_count;

        if (c == '/') {
            int next = get_char(file);
            if (next == '/') {
                while ((c = get_char(file)) != '\n' && c != EOF);
                if (c == '\n') {
                    line_count++;
                    col_count = 0;
                }
                continue;
            } else if (next == '*') {
                while (true) {
                    c = get_char(file);
                    if (c == EOF) {
                        handle_lexical_error(file, "Fim de arquivo em comentario", '\0');
                        return;
                    }
                    if (c == '\n') {
                        line_count++;
                        col_count = 0;
                    }
                    if (c == '*') {
                        if ((next = get_char(file)) == '/') break;
                        else unget_char(next, file);
                    }
                }
                continue;
            } else {
                unget_char(next, file);
                printf("[%02d:%02d] TK_OPERATOR : /\n", line_count, start_col);
                continue;
            }
        }

        if (c == '"') {
            char buf[512] = {0};
            int i = 0;
            bool closed = false;
            
            while ((c = get_char(file)) != EOF) {
                if (c == '\\') {
                    int next = get_char(file);
                    if (next == '"') {
                        if (i < 511) buf[i++] = '"';
                        continue;
                    } else {
                        if (i < 511) buf[i++] = '\\';
                        unget_char(next, file);
                        continue;
                    }
                }
                if (c == '"') {
                    closed = true;
                    break;
                }
                if (c == '\n') {
                    handle_lexical_error(file, "String nao fechada", '\0');
                    line_count++;
                    col_count = 0;
                    goto next_token;
                }
                if (i < 511) buf[i++] = (char)c;
            }
            
            if (closed) {
                printf("[%02d:%02d] TK_STRING   : \"%s\"\n", line_count, start_col, buf);
            } else {
                handle_lexical_error(file, "Fim de arquivo em string", '\0');
            }
            continue;
        }

        if (c == '\'') {
            char content[10] = {0};
            int i = 0;
            bool closed = false;
            while ((c = get_char(file)) != EOF) {
                if (c == '\'') { closed = true; break; }
                if (c == '\n') {
                    handle_lexical_error(file, "Char nao fechado", '\0');
                    line_count++;
                    col_count = 0;
                    goto next_token;
                }
                if (i < 9) content[i++] = (char)c;
            }
            if (closed && i > 0) printf("[%02d:%02d] TK_CHAR       : '%s'\n", line_count, start_col, content);
            continue;
        }

        if (isalpha(c) || c == '_') {
            char buf[128] = {(char)c}; int i = 1;
            while (isalnum(c = get_char(file)) || c == '_') {
                if (i < 127) buf[i++] = (char)c;
            }
            buf[i] = '\0';
            unget_char(c, file);
            if (is_keyword(buf)) printf("[%02d:%02d] TK_KEYWORD  : %s\n", line_count, start_col, buf);
            else printf("[%02d:%02d] TK_ID       : %s\n", line_count, start_col, buf);
            continue;
        }

        if (isdigit(c)) {
            char buf[64] = {(char)c}; int i = 1; bool is_f = false;
            while (isdigit(c = get_char(file)) || c == '.') {
                if (c == '.') {
                    if (is_f) break;
                    is_f = true;
                }
                if (i < 63) buf[i++] = (char)c;
            }
            buf[i] = '\0';
            unget_char(c, file);
            printf("[%02d:%02d] %-11s: %s\n", line_count, start_col, is_f ? "TK_FLOAT" : "TK_INT", buf);
            continue;
        }

        if (strchr("+-=><!&|", c)) {
            int next = get_char(file);
            char op[3] = {(char)c, '\0', '\0'};
            if ((c == '+' && next == '+') || (c == '-' && next == '-') || 
                (c == '=' && next == '=') || (c == '>' && next == '=') || 
                (c == '<' && next == '=') || (c == '!' && next == '=') ||
                (c == '&' && next == '&') || (c == '|' && next == '|')) {
                op[1] = (char)next;
                printf("[%02d:%02d] TK_OPERATOR : %s\n", line_count, start_col, op);
            } else {
                unget_char(next, file);
                printf("[%02d:%02d] TK_OPERATOR : %c\n", line_count, start_col, c);
            }
            continue;
        }

        if (strchr("(){};,*[]", c)) {
            printf("[%02d:%02d] TK_SPECIAL  : %c\n", line_count, start_col, c);
            continue;
        }

        handle_lexical_error(file, "Caractere invalido", (char)c);
        next_token:;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <arquivo.c>\n", argv[0]);
        return 1;
    }
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("Erro ao abrir arquivo");
        return 1;
    }
    process_file(file);
    fclose(file);
    return 0;
}
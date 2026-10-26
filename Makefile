CC       = gcc
CFLAGS   = -Wall -Wextra -g -O0 -std=c11 -Iinclude -I$(BIN)
FLEX     = flex
BISON    = bison
VALGRIND = valgrind --leak-check=full --error-exitcode=9

BIN      = output

CORE_HDRS = include/common.h include/unitvec.h
FE_HDRS   = include/ast.h $(CORE_HDRS)
ST_HDRS   = include/symtab.h $(FE_HDRS)

.PHONY: all test clean valgrind vec_test symtab_test fe

all: $(BIN)/vec_test $(BIN)/symtab_test $(BIN)/dimc

$(BIN):
	@mkdir -p $(BIN)

# ---------------- Ayansh: vector engine ----------------
$(BIN)/unitvec.o: src/unitvec.c $(CORE_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/vec_test: src/vec_test.c $(BIN)/unitvec.o
	$(CC) $(CFLAGS) $^ -o $@

vec_test: $(BIN)/vec_test
	./$(BIN)/vec_test

# ---------------- Balraj: symbol table + front end ----------------
$(BIN)/symtab.o: src/symtab.c $(ST_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/symtab_test: src/symtab_test.c $(BIN)/symtab.o $(BIN)/unitvec.o
	$(CC) $(CFLAGS) $^ -o $@

symtab_test: $(BIN)/symtab_test
	./$(BIN)/symtab_test

$(BIN)/ast.o: src/ast.c $(FE_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

# bison: generates parser.tab.c AND parser.tab.h as a side effect
$(BIN)/parser.tab.o: src/parser.y $(FE_HDRS) | $(BIN)
	$(BISON) -d -o $(BIN)/parser.tab.c $<
	$(CC) $(CFLAGS) -c $(BIN)/parser.tab.c -o $@

# flex: lexer world includes the generated header
$(BIN)/lexer.yy.o: src/lexer.l $(BIN)/parser.tab.o | $(BIN)
	$(FLEX) -o$(BIN)/lexer.yy.c $<
	$(CC) $(CFLAGS) -c $(BIN)/lexer.yy.c -o $@

$(BIN)/frontend_main.o: src/frontend_main.c $(ST_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/dimc: $(BIN)/lexer.yy.o $(BIN)/parser.tab.o $(BIN)/ast.o \
            $(BIN)/symtab.o $(BIN)/unitvec.o $(BIN)/frontend_main.o
	$(CC) $(CFLAGS) -o $@ $^

# front end only: parse + AST + symbol table dump (Balraj's milestone)
fe: $(BIN)/dimc
	./$(BIN)/dimc $(FILE)

test: vec_test symtab_test
	@echo "all contract tests passed"

# ---------------- QA (run under WSL/Linux, not Windows cmd) ----------------
valgrind: all
	$(VALGRIND) ./$(BIN)/vec_test
	$(VALGRIND) ./$(BIN)/symtab_test
	$(VALGRIND) ./$(BIN)/dimc demo/basic_fe_check.dim

clean:
	rm -rf $(BIN)

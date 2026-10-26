CC       = gcc
CFLAGS   = -Wall -Wextra -g -O0 -std=c11 -Iinclude -I$(BIN)
FLEX     = flex
BISON    = bison
VALGRIND = valgrind --leak-check=full --error-exitcode=9

BIN      = output

CORE_HDRS = include/common.h include/unitvec.h
FE_HDRS   = include/ast.h $(CORE_HDRS)
ST_HDRS   = include/symtab.h $(FE_HDRS)

.PHONY: all test clean valgrind vec_test symtab_test fe checker pipeline

# Shreyansh's src/main.c + src/codegen.c are wildcard-guarded: the moment
# both exist, `make`/`make all` builds the unified ./dimc too. Until
# src/main.c lands, `all` skips it.
HAS_BE := $(wildcard src/main.c)$(wildcard src/codegen.c)

all: $(BIN)/vec_test $(BIN)/symtab_test $(BIN)/semantic_test $(BIN)/dimc-checker \
     $(if $(HAS_BE),$(BIN)/dimc)

$(BIN):
	@mkdir -p $(BIN)

# ---------------- Ayansh: vector engine + semantic analyzer ----------------
$(BIN)/unitvec.o: src/unitvec.c $(CORE_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/vec_test: src/vec_test.c $(BIN)/unitvec.o
	$(CC) $(CFLAGS) $^ -o $@

vec_test: $(BIN)/vec_test
	./$(BIN)/vec_test

SEM_HDRS = include/semantic.h $(ST_HDRS)

$(BIN)/semantic.o: src/semantic.c $(SEM_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/semantic_ops.o: src/semantic_ops.c $(SEM_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/semantic_test: src/semantic_test.c $(BIN)/semantic.o \
                      $(BIN)/semantic_ops.o $(BIN)/symtab.o $(BIN)/unitvec.o \
                      $(BIN)/ast.o
	$(CC) $(CFLAGS) $^ -o $@

semantic_test: $(BIN)/semantic_test
	./$(BIN)/semantic_test

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

$(BIN)/frontend_main.o: src/frontend_main.c $(ST_HDRS) $(SEM_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

# checker build: same skeleton WITH its own main() and debug dumps
$(BIN)/frontend_checker.o: src/frontend_main.c $(ST_HDRS) $(SEM_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -DBUILD_CHECKER -c $< -o $@

$(BIN)/dimc-checker: $(BIN)/lexer.yy.o $(BIN)/parser.tab.o $(BIN)/ast.o \
            $(BIN)/symtab.o $(BIN)/unitvec.o $(BIN)/semantic.o \
            $(BIN)/semantic_ops.o $(BIN)/frontend_checker.o
	$(CC) $(CFLAGS) -o $@ $^

# final unified pipeline (Shreyansh's main.c + codegen.c + everything).
# Building this before src/main.c exists fails with the missing-file error —
# which is the cue to run `make checker` (front end + analyzer only) instead.
$(BIN)/main.o: src/main.c $(SEM_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

CG_HDRS = include/codegen.h $(SEM_HDRS)
$(BIN)/codegen.o: src/codegen.c $(CG_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/dimc: $(BIN)/lexer.yy.o $(BIN)/parser.tab.o $(BIN)/ast.o \
            $(BIN)/symtab.o $(BIN)/unitvec.o $(BIN)/semantic.o \
            $(BIN)/semantic_ops.o $(BIN)/frontend_main.o \
            $(BIN)/codegen.o $(BIN)/main.o
	$(CC) $(CFLAGS) -o $@ $^

# Phase-3 checker (parse + AST + analyzer), no codegen yet. old alias kept:
checker: $(BIN)/dimc-checker
	./$(BIN)/dimc-checker $(FILE)
fe: checker

test: vec_test symtab_test semantic_test
	@echo "all contract tests passed"

# ---------------- Shreyansh: test suite ----------------
tests:
	./tests/run_tests.sh

# ---------------- QA (run under WSL/Linux, not Windows cmd) ----------------
valgrind: all
	$(VALGRIND) ./$(BIN)/vec_test
	$(VALGRIND) ./$(BIN)/symtab_test
	$(VALGRIND) ./$(BIN)/dimc-checker tests/valid/00_basic_fe_check.dim

clean:
	rm -rf $(BIN)

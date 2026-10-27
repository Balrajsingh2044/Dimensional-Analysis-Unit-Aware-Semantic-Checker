CC       = gcc
CFLAGS   = -Wall -Wextra -g -O0 -std=c11 -Iinclude -I$(BIN)
LDLIBS   = -lm   # pow() in unitvec.c/codegen.c; MinGW ships a harmless stub
FLEX     = flex
BISON    = bison
VALGRIND = valgrind --leak-check=full --error-exitcode=9

BIN      = output

CORE_HDRS = include/common.h include/unitvec.h
FE_HDRS   = include/ast.h $(CORE_HDRS)
ST_HDRS   = include/symtab.h $(FE_HDRS)

.PHONY: all test clean valgrind vec_test symtab_test fe checker pipeline audit audit-body

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
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

vec_test: $(BIN)/vec_test
	./$(BIN)/vec_test

SEM_HDRS = include/semantic.h $(ST_HDRS)

# Week-5 interpreter pass (evaluator.c / evaluator.h)
EVAL_HDRS = include/evaluator.h $(SEM_HDRS)

$(BIN)/semantic.o: src/semantic.c $(SEM_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/semantic_ops.o: src/semantic_ops.c $(SEM_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/evaluator.o: src/evaluator.c $(EVAL_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/semantic_test: src/semantic_test.c $(BIN)/semantic.o \
                      $(BIN)/semantic_ops.o $(BIN)/symtab.o $(BIN)/unitvec.o \
                      $(BIN)/ast.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

semantic_test: $(BIN)/semantic_test
	./$(BIN)/semantic_test

# ---------------- Balraj: symbol table + front end ----------------
$(BIN)/symtab.o: src/symtab.c $(ST_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/symtab_test: src/symtab_test.c $(BIN)/symtab.o $(BIN)/unitvec.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

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
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# final unified pipeline (Shreyansh's main.c + codegen.c + everything).
# Building this before src/main.c exists fails with the missing-file error —
# which is the cue to run `make checker` (front end + analyzer only) instead.
$(BIN)/main.o: src/main.c $(EVAL_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

CG_HDRS = include/codegen.h $(SEM_HDRS)
$(BIN)/codegen.o: src/codegen.c $(CG_HDRS) | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/dimc: $(BIN)/lexer.yy.o $(BIN)/parser.tab.o $(BIN)/ast.o \
            $(BIN)/symtab.o $(BIN)/unitvec.o $(BIN)/semantic.o \
            $(BIN)/semantic_ops.o $(BIN)/frontend_main.o \
            $(BIN)/codegen.o $(BIN)/evaluator.o $(BIN)/main.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

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

# ---------------- portable leak audit (Balraj, final QA) ----------------
# Works on ANY machine with gcc (Windows included — no valgrind/WSL needed):
# same 5 contexts as Ayansh's HOWTO table, in-use-at-exit metric identical.
# alloc_audit.c must be compiled WITHOUT the -include flag (self-wrap = recursion).
# Linux fallback where valgrind is missing, and cross-check where it isn't.
AUDIT_FLAGS = $(CFLAGS) -include src/alloc_audit.h -DALLOC_AUDIT

.PHONY: audit-body
audit-body: $(BIN)/audit/vec_test $(BIN)/audit/symtab_test $(BIN)/audit/semantic_test \
            $(BIN)/audit/dimc-checker $(BIN)/audit/dimc

$(BIN)/audit $(BIN)/audit/:
	mkdir -p $(BIN)/audit

$(BIN)/audit/alloc_audit.o: src/alloc_audit.c | $(BIN)/audit
	$(CC) $(CFLAGS) -c src/alloc_audit.c -o $@

$(BIN)/audit/frontend_checker.o: src/frontend_main.c $(wildcard include/*.h) src/alloc_audit.h | $(BIN)/audit
	$(CC) $(AUDIT_FLAGS) -DBUILD_CHECKER -c $< -o $@

$(BIN)/audit/lexer.yy.o: $(BIN)/lexer.yy.c $(wildcard include/*.h) src/alloc_audit.h | $(BIN)/audit
	$(CC) $(AUDIT_FLAGS) -c $< -o $@

$(BIN)/audit/parser.tab.o: $(BIN)/parser.tab.c $(wildcard include/*.h) src/alloc_audit.h | $(BIN)/audit
	$(CC) $(AUDIT_FLAGS) -c $< -o $@

$(BIN)/audit/%.o: src/%.c $(wildcard include/*.h) src/alloc_audit.h | $(BIN)/audit
	$(CC) $(AUDIT_FLAGS) -c $< -o $@

$(BIN)/audit/vec_test: $(BIN)/audit/vec_test.o $(BIN)/audit/unitvec.o \
                       $(BIN)/audit/alloc_audit.o
	$(CC) $(AUDIT_FLAGS) -o $@ $^ $(LDLIBS)

$(BIN)/audit/symtab_test: $(BIN)/audit/symtab_test.o $(BIN)/audit/symtab.o \
                          $(BIN)/audit/unitvec.o $(BIN)/audit/alloc_audit.o
	$(CC) $(AUDIT_FLAGS) -o $@ $^ $(LDLIBS)

$(BIN)/audit/semantic_test: $(BIN)/audit/semantic_test.o $(BIN)/audit/semantic.o \
                            $(BIN)/audit/semantic_ops.o $(BIN)/audit/symtab.o \
                            $(BIN)/audit/unitvec.o $(BIN)/audit/ast.o \
                            $(BIN)/audit/alloc_audit.o
	$(CC) $(AUDIT_FLAGS) -o $@ $^ $(LDLIBS)

$(BIN)/audit/dimc-checker: $(BIN)/audit/lexer.yy.o $(BIN)/audit/parser.tab.o \
                           $(BIN)/audit/ast.o $(BIN)/audit/symtab.o \
                           $(BIN)/audit/unitvec.o $(BIN)/audit/semantic.o \
                           $(BIN)/audit/semantic_ops.o $(BIN)/audit/frontend_checker.o \
                           $(BIN)/audit/alloc_audit.o
	$(CC) $(AUDIT_FLAGS) -o $@ $^ $(LDLIBS)

$(BIN)/audit/dimc: $(BIN)/audit/lexer.yy.o $(BIN)/audit/parser.tab.o \
                   $(BIN)/audit/ast.o $(BIN)/audit/symtab.o $(BIN)/audit/unitvec.o \
                   $(BIN)/audit/semantic.o $(BIN)/audit/semantic_ops.o \
                   $(BIN)/audit/frontend_main.o $(BIN)/audit/codegen.o \
                   $(BIN)/audit/evaluator.o $(BIN)/audit/main.o \
                   $(BIN)/audit/alloc_audit.o
	$(CC) $(AUDIT_FLAGS) -o $@ $^ $(LDLIBS)

# The five report contexts from docs/valgrind_baseline_HOWTO.md + extras:
audit: audit-body
	./$(BIN)/audit/vec_test
	./$(BIN)/audit/symtab_test
	./$(BIN)/audit/semantic_test
	./$(BIN)/audit/dimc-checker tests/valid/00_basic_fe_check.dim
	./$(BIN)/audit/dimc-checker tests/invalid/01_dist_plus_time.dim
	./$(BIN)/audit/dimc-checker tests/invalid/04_metric_imperial.dim
	./$(BIN)/audit/dimc-checker tests/invalid/05_missing_semicolon.dim
	./$(BIN)/audit/dimc.exe demo/mars_valid.dim
	./$(BIN)/audit/dimc.exe tests/invalid/04_metric_imperial.dim
	./$(BIN)/audit/dimc.exe -r demo/mars_valid.dim

clean:
	rm -rf $(BIN)

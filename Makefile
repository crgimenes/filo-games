# filo-games: games and demos written in Filo, each its own program — a
# bundle (NAME.fbb) any Filo VM with the app's base can load, the board's
# among them, and a binary (bin/NAME) that carries it.
CC ?= cc
CLANG_FORMAT ?= clang-format
CLANG_TIDY ?= clang-tidy
FILO_TERM ?= ../filo-term
FILO ?= ../clang_filo
# The compiler of the games: C's, built from FILO, unless another is named.
# Go's filo writes the same bytes (make FILO_CLI=filo).
FILO_CLI ?= $(FILO)/build/filo
CLI_DEP = $(filter $(FILO)/build/filo,$(FILO_CLI))

GAMES = donut snake down fire

WARN = -Wall -Wextra -Werror -Wshadow -Wconversion -Wdouble-promotion -Wundef
VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)
INC = -I$(FILO_TERM)/src -I$(FILO)
FLAGS = -std=c11 -D_DEFAULT_SOURCE $(WARN) $(INC) -DGAME_VERSION='"$(VERSION)"' -DFILO_VM_ONLY
LIBS = $(addprefix $(FILO_TERM)/src/,term.c canvas.c utf8.c keyin.c paint.c field.c app.c)
FILOSRC = $(addprefix $(FILO)/,filo.c filo_math.c filo_strings.c filo_nolibc.c)
HDRS = $(wildcard $(FILO_TERM)/src/*.h) $(FILO)/filo.h
# A release links everything it can: LDFLAGS=-static on Linux (musl); macOS
# has no static libc, and a game needs nothing past libSystem anyway.
LDFLAGS ?=
PREFIX ?= /usr/local

.PHONY: all test fmt fmt-check tidy check qa install clean

BINS = $(addprefix bin/,$(GAMES))

all: $(BINS) $(GAMES:=.fbb)

$(FILO)/build/filo:
	$(MAKE) -C $(FILO) build/filo

# What a game's binary gives its bundle: the app's base, listed by the
# app's own code. Every game is compiled against it and held to it.
build/games.vm: vm.c $(LIBS) $(FILOSRC) $(FILO_TERM)/tools/appvm.c $(HDRS)
	@mkdir -p build
	$(CC) -O1 $(FLAGS) -o build/appvm vm.c $(LIBS) $(FILOSRC) $(FILO_TERM)/tools/appvm.c
	./build/appvm > $@

# One entry per file of the game's directory, one member named as the game.
.SECONDEXPANSION:
$(GAMES:=.fbb): %.fbb: $$(wildcard %/*.filo) build/games.vm $(CLI_DEP)
	@mkdir -p build
	$(FILO_CLI) build -vm build/games.vm -o build/$*.fbc $(wildcard $*/*.filo)
	$(FILO_CLI) bundle -o $@ build/$*.fbc
	$(FILO_CLI) check -vm build/games.vm $@

build/%_fbb.c: %.fbb
	sh $(FILO_TERM)/tools/embed.sh $*_fbb $< > $@

$(BINS): bin/%: %/main.c build/%_fbb.c $(LIBS) $(FILOSRC) $(FILO_TERM)/src/tty.c $(HDRS)
	@mkdir -p bin
	$(CC) -O2 $(FLAGS) -o $@ $*/main.c build/$*_fbb.c $(LIBS) $(FILOSRC) $(FILO_TERM)/src/tty.c \
		$(LDFLAGS)

# Each game played for a while under the sanitizers, within a board's budgets.
test: $(GAMES:=.fbb) test/test_games.c $(LIBS) $(FILOSRC) $(HDRS)
	@mkdir -p build
	$(CC) -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all $(FLAGS) \
		-o build/test_games test/test_games.c $(LIBS) $(FILOSRC)
	./build/test_games $(GAMES:=.fbb)

SRC = vm.c $(wildcard */main.c) test/test_games.c

fmt:
	$(CLANG_FORMAT) -i $(SRC)

fmt-check:
	$(CLANG_FORMAT) --dry-run --Werror $(SRC)

TIDY_CHECKS = bugprone-*,cert-*,clang-analyzer-*,readability-*,-readability-magic-numbers,-readability-identifier-length,-bugprone-easily-swappable-parameters,-cert-err33-c,-readability-else-after-return,-clang-analyzer-optin.performance.Padding

tidy:
	$(CLANG_TIDY) --quiet --warnings-as-errors='*' --checks='$(TIDY_CHECKS)' $(SRC) \
		-- -std=c11 -D_DEFAULT_SOURCE $(INC) -DGAME_VERSION='"x"'

check:
	cppcheck --enable=warning,style,performance,portability --inline-suppr \
		--suppress=missingIncludeSystem --error-exitcode=1 $(INC) -DGAME_VERSION='"x"' $(SRC)

qa: all fmt-check test tidy check

install: $(BINS)
	mkdir -p $(PREFIX)/bin
	cp $(BINS) $(PREFIX)/bin/

clean:
	rm -rf build bin $(GAMES:=.fbb)

SHELL      = /bin/sh
CC         = /usr/bin/gcc
VER       = $(shell cat VERSION)


.PHONY: all testing debug verbose install uninstall clean

all: bin doc

testing: bin

debug: bin

verbose: bin

install: install-bin install-doc install-config

uninstall: uninstall-bin uninstall-doc uninstall-config

clean: clean-bin clean-doc

############################
# Building the Application #
############################
CFLAGS    = -Wall -D 'VERSION="$(VER)"' -I /usr/include/freetype2

ifneq (,$(findstring debug, $(MAKECMDGOALS)))
CFLAGS   += -g
else
CFLAGS   += -O2
endif

ifneq (,$(findstring testing, $(MAKECMDGOALS)))
CFLAGS   += -D 'TESTING'
endif

ifneq (,$(findstring verbose, $(MAKECMDGOALS)))
CFLAGS   += -D 'VERBOSE'
endif
	
THOR_LIBS = -lxcb -lxcb-shape -lcairo -lrt -pthread -lfontconfig
_THOR_OBJ = com.o config.o drawing.o logging.o NotificaThor.o theme.o utils.o wins.o images.o text.o
THOR_OBJ  = $(addprefix obj/, $(_THOR_OBJ))

BIN_PATH  = $(prefix)/usr/
BINARIES  = bin/notificathor bin/thor-cli
BIN_INST  = $(BINARIES:%=$(BIN_PATH)%)


.PHONY: bin install-bin uninstall-bin clean-bin

bin: $(BINARIES)

install-bin: $(BIN_INST)

uninstall-bin:
	@echo "Removing binaries..."
	@-rm -f $(BIN_INST)

clean-bin:
	@echo "Cleaning binaries..."
	@-rm -rf deps/
	@-rm -rf obj/
	@-rm -rf bin/

# Link NotificaThor
bin/notificathor: $(THOR_OBJ) $(filter-out $(wildcard bin/), bin/)
	@echo "Building $@..."
	@$(CC) $(THOR_LIBS) $(filter-out bin/, $^) -o $@

# Link thor-cli
bin/thor-cli: obj/thor-cli.o $(filter-out $(wildcard bin/), bin/)
	@echo "Building $@..."
	@$(CC) $< -o $@

# Compile sources, create dependency files
obj/%.o: src/%.c $(filter-out $(wildcard obj/), obj/) $(filter-out $(wildcard deps/), deps/) VERSION
	@echo "Compiling $@..."
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(CC) $(CFLAGS) -M -MP $< | sed -e "1 s/^/obj\//" > deps/$*.d
	@echo -e "\n$<:" >> deps/$*.d

# Include dependency files
-include $(wildcard deps/*.d)



######################
# Generate man-pages #
######################
DOC_TMP  = $(basename $(wildcard doc/*.source))
DOC_COMP  = $(DOC_TMP:%=%.gz)

MAN_PATH  = $(prefix)/usr/share/man/man
DOC_INST  = $(foreach tmp, $(notdir $(DOC_TMP)), $(MAN_PATH)$(subst .,,$(suffix $(tmp)))/$(tmp).gz)


.PHONY: doc install-doc uninstall-doc clean-doc

doc: $(DOC_COMP)

install-doc: $(DOC_INST)

uninstall-doc:
	@echo "Removing man pages..."
	@-rm -f $(DOC_INST)

clean-doc:
	@echo "Cleaning docs..."
	@-rm -f $(DOC_COMP) $(DOC_TMP)

# Insert Version
$(DOC_TMP): %: %.source VERSION
	@echo "Inserting version to $@..."
	@sed -e "s/%%VERSION%%/$(VER)/" $< > $@

# Compress man page
$(DOC_COMP): %.gz: %
	@echo "Compressing $@..."
	@gzip -c $< > $@

##################
# Install config #
##################
CONF_PATH = $(prefix)/etc/NotificaThor/


.PHONY: install-config uninstall-config
install-config: $(CONF_PATH)
	@echo "Installing config files..."
	@cp -r etc/NotificaThor/* $(CONF_PATH)

uninstall-config:
	@echo "Removing $(CONF_PATH)..."
	@-rm -rf $(CONF_PATH)

##################
# Create folders #
##################

.PRECIOUS: %/
%/:
	@mkdir $@


###############################
# Second Expansion targets #
###############################

# Installing man page
.SECONDEXPANSION:
$(DOC_INST): %: doc/$$(notdir %) $$(dir %)
	@echo "Installing $@..."
	@cp $< $@

# Installing binary
$(BIN_INST): %: bin/$$(notdir %)
	@echo "Installing $@..."
	@cp $^ $@

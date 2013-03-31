
DOC_SRC  = NotificaThor.1 thor-cli.1 NotificaThor-themes.5
DOC_TMP  = $(DOC_SRC:=.source)
DOC_COMP = $(DOC_SRC:=.gz)
VER      = $(shell cat ../VERSION)

.PHONY: all clean

all: $(DOC_COMP)

clean:
	rm $(DOC_COMP)
	rm $(DOC_TMP)

# compress man pages
$(DOC_COMP): %.gz: %.source
	gzip -c $^ > $@

# insert version
$(DOC_TMP): %.source: %
	sed "s/%%VERSION%%/$(VER)/" $^ > $@

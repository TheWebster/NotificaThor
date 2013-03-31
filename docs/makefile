DOC_SRC  = NotificaThor.1 thor-cli.1 NotificaThor-themes.5
DOC_COMP = $(DOC_SRC:=.gz)

.PHONY: all clean

all: $(DOC_COMP)

clean:
	rm *.gz

$(DOC_COMP): %.gz: %
	gzip -c $^ > $@

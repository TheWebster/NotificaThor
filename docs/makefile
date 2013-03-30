
all: NotificaThor.1 thor-cli.1 NotificaThor-themes.5
	gzip -c NotificaThor.1 > NotificaThor.1.gz
	gzip -c thor-cli.1 > thor-cli.1.gz
	gzip -c NotificaThor-themes.5 > NotificaThor-themes.5.gz

clean:
	rm *.gz

.PHONY: all clean
	

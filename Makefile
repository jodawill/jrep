NAME = jrep
CC=cc
OBJECTS = src/jrep.o src/regex.o
DEPS = src/jrep.h

.PHONY: clean make install uninstall

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $<

$(NAME): $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS)

install:
	cp $(NAME) /usr/bin/$(NAME)

uninstall:
	rm /usr/bin/$(NAME)

clean:
	rm -f $(OBJECTS) $(NAME)

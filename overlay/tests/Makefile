CC = gcc
CFLAGS = -Wall -Wextra -O0 -g \
         $(shell pkg-config --cflags cairo 2>/dev/null) \
         $(shell pkg-config --cflags libcjson 2>/dev/null)
LDFLAGS_CAIRO = $(shell pkg-config --libs cairo 2>/dev/null)
LDFLAGS_CJSON = $(shell pkg-config --libs libcjson 2>/dev/null)

TESTS = test_card_stack test_toast test_permission_ui test_session_switcher test_ipc_buffer

.PHONY: all clean run

all: $(TESTS)

test_card_stack: test_card_stack.c ../card_stack.c
	$(CC) $(CFLAGS) -o $@ test_card_stack.c ../card_stack.c

test_toast: test_toast.c ../toast.c render_stub.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_CAIRO)

test_permission_ui: test_permission_ui.c ../permission_ui.c render_stub.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_CAIRO)

test_session_switcher: test_session_switcher.c ../session_switcher.c render_stub.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_CAIRO) $(LDFLAGS_CJSON)

test_ipc_buffer: test_ipc_buffer.c ../ipc.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_CJSON)

run: all
	@echo "=== Running C unit tests ==="
	@failed=0; \
	for t in $(TESTS); do \
		echo ""; \
		./$$t || failed=1; \
	done; \
	echo ""; \
	if [ $$failed -eq 0 ]; then echo "All test suites passed."; else echo "SOME TESTS FAILED"; exit 1; fi

clean:
	rm -f $(TESTS)

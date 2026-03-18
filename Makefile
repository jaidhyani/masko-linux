.PHONY: all overlay backend test test-c clean

all: overlay

overlay:
	$(MAKE) -C overlay

test:
	cd masko-linux && .venv/bin/python -m pytest tests/ -v

test-c:
	$(MAKE) -C overlay/tests run

clean:
	$(MAKE) -C overlay clean

venv:
	python3 -m venv .venv
	.venv/bin/pip install -r requirements.txt

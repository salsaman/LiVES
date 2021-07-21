SCRIPT_FILES = *.script

install: generate dist

dist: all-scripts
	for script in $(SCRIPT_FILES); do \
		./build-weed-plugin-C $$script; \
	done

all: generate

generate: build-weed-plugin-C

build-weed-plugin-C: $(SCRIPT_FILES)
	@touch $@
	for script in $?; do \
		./$@ $$script ..;\
	done

all-scripts: $(SCRIPT_FILES)
	for script in $^; do \
		./build-weed-plugin-C $$script ..;\
		touch $$script; \
	done
	touch build-weed-plugin-C

EXTRA_DIST = build-weed-plugin-C $(SCRIPT_FILES)

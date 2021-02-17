SCRIPT_FILES = *.script

all: generate

generate: build-weed-plugin-C

build-weed-plugin-C: $(SCRIPT_FILES)
	for script in $?; do \
		./$@ $$script .. ;\
		touch $$script ; \
	done
	@touch $@

all-scripts: $(SCRIPT_FILES)
	for script in $^; do \
		./build-weed-plugin-C $$script .. ;\
		touch $$script ; \
	done
	touch build-weed-plugin-C

EXTRA_DIST = build-weed-plugin-C $(SCRIPT_FILES)

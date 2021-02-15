SCRIPT_FILES = *.script

all: generate

generate: build-weed-plugin-C $(SCRIPT_FILES)

build-weed-plugin-C: $(SCRIPT_FILES)
	for script in $^; do \
		./$@ $$script .. ;\
		touch $$script ; \
	done
	touch $@

EXTRA_DIST = build-weed-plugin-C $(SCRIPT_FILES)

SCRIPT_FILES = *.script

all: generate

generate: $(SCRIPT_FILES)
	for script in $^; do \
		./build-weed-plugin-C $$script .. ;\
		touch $$script ; \
	done

#.PHONY : all


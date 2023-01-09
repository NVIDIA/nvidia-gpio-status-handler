jsonGenerator := json-gen.py

datOrigTemplate := dat/original/dat-template.json
datOrigGenerated := dat/original/dat-generated.json
datOrigGeneratedNorm := tmp/dat-generated-norm.json

datOrigNorm := tmp/dat-original-norm.json

# normalizeJson(original,normalized)
define normalizeJson
$(2): $(1) | $$(dir $(2))
	cat $(1) | jq --sort-keys > $$@
endef

.PHONY: all
all: .gitignore diffTest

diffTest: $(datOrigGeneratedNorm) $(datOrigNorm)
	diff --report-identical-files $(datOrigGeneratedNorm) $(datOrigNorm)

$(datOrigGeneratedNorm): $(datOrigGenerated) | $(dir $(datOrigGeneratedNorm))
	cat $(datOrigGenerated) | jq --sort-keys > $@

$(datOrigNorm): $(datOrig) | $(dir $(datOrigNorm))
ifdef datOrig
	cat $(datOrig) | jq --sort-keys > $@
else
	@echo "'datOrig' variable is not defined. Point it to the original dat.json to compare with"
	exit 1
endif

$(datOrigGenerated): $(datOrigTemplate) $(jsonGenerator)
	python3 $(jsonGenerator) $(datOrigTemplate) > $@

%/:
	mkdir $@

define gitignoreContent
__pycache__/*
tmp/*
$(datOrigGenerated)
endef

export gitignoreContent
.gitignore: alwaysUpdate
	echo "$$gitignoreContent" > $@

.PHONY: alwaysUpdate
alwaysUpdate: 

.PHONY: clean
clean:
	rm -rf tmp $(datOrigGenerated)

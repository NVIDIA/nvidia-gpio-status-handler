indent := "    "

# Tools #######################################################################

define ensureDir
$(1): | $$(call dir,$(1))
endef

define makefileDep
$(1): $(MAKEFILE_LIST)
endef

define cleanable
toClean += $(1)
endef

%/:
	mkdir -p $@

define standardTarget
$$(eval $$(call ensureDir,$(1)))
$$(eval $$(call makefileDep,$(1)))
$$(eval $$(call cleanable,$(1)))
endef

# Targets #####################################################################

# Scripts
srcDir        := src
testpJq       := $(srcDir)/jq/testpoints.jq
tabTestpJq    := $(srcDir)/jq/tabular-testpoints.jq
tabTestpCmdJq := $(srcDir)/jq/tabular-testpoints-CMDLINE.jq
genWrappers   := $(srcDir)/python/generate-wrappers.py
genTemplate   := $(srcDir)/python/make-wrappers-mockup-definition-template.py
jsonToCsv     := $(srcDir)/python/json-to-csv.py
deployProg    := $(srcDir)/bash/deployment-script.sh

# Input
# DAT
wrappersDefTempl := wrappers-mockup-definition-template.tsv
stdTargets += $(wrappersDefTempl)

# Intermediaries and outputs
outDir      := data
toClean += $(outDir)

testpDir    := $(outDir)/testpoints
testpCmdDir := $(outDir)/testpoints-CMDLINE
extraDir    := $(outDir)/extra

datTabJson   := $(testpDir)/tabular-testpoints.json
testpTabJson := $(testpCmdDir)/tabular-testpoints-CMDLINE.json

testpTsv    := $(testpDir)/testpoints.tsv
testpCmdTsv := $(testpCmdDir)/testpoints-CMDLINE.tsv

allAccPropsList := $(extraDir)/all-accessor-properties.json
allAccTypesList := $(extraDir)/all-accessor-types.json
allDevices      := $(extraDir)/all-devices.json

stdTargets += $(datTabJson) $(testpTabJson) $(testpTsv) \
	$(testpCmdTsv) $(allAccPropsList) $(allAccTypesList)

extras := $(allAccPropsList) $(allAccTypesList) $(allDevices)

noDATerrMsg := @ {                                       \
    echo;                                                \
    echo "$(indent)ERROR: 'DAT' variable must be defined \
containing path to the dat.json file used by selftest."; \
    echo;                                                \
    exit 1;                                              \
}

# comment #####################################################################

deployDir := wrapper-mockups-deploy-pkg
toClean += $(deployDir)

wrappersDir      := $(deployDir)/wrapper-mockups
deployTargetProg := $(deployDir)/deploy.sh
deployPkg        := $(deployDir).tgz

stdTargets += $(deployTargetProg) $(deployPkg)

$(deployPkg): deployTree alwaysUpdate
	tar --dereference --create --file $(deployPkg) --auto-compress $(deployDir)
	@ echo "$(indent)"
	@ echo "$(indent)Package '$(deployPkg)' created. Copy it to OpenBMC, then, inside:"
	@ echo "$(indent)"
	@ echo "$(indent)    tar -xf $(deployPkg)"
	@ echo "$(indent)    $(deployTargetProg)"
	@ echo "$(indent)"
	@ echo "$(indent)It will substitute the mockup wrappers for the original ones. Run"
	@ echo "$(indent)"
	@ echo "$(indent)    $(deployTargetProg) original"
	@ echo "$(indent)"
	@ echo "$(indent)to return to the original wrappers"
	@ echo "$(indent)"
stdTarget += $(deployPkg)

.PHONY: all
all: wrappers template analysis $(deployPkg)

.PHONY: deployTree
deployTree: $(deployTargetProg) wrappers

# phony target will always trigger the update
.PHONY: alwaysUpdate
alwaysUpdate:

$(deployTargetProg): | $(deployProg)
	ln --force --symbolic --relative $(deployProg) $@
	chmod u+x $@

# # ###########################################################################

.PHONY: wrappers
ifdef WRAPPERS_DEF
wrappers: $(genWrappers) $(WRAPPERS_DEF) | $(wrappersDir)
	$(genWrappers) $(WRAPPERS_DEF) $(wrappersDir)
	chmod u+x $(wrappersDir)/*
else
wrappers: $(genWrappers) | $(wrappersDefTempl) $(wrappersDir)
	@ echo ""
	@ echo "$(indent)WARNING: No 'WRAPPERS_DEF' variable defined. It should contain"
	@ echo "$(indent)path to a TSV wrappers definition file. Using the default,"
	@ echo "$(indent)auto-generated '$(wrappersDefTempl)' file (all wrappers returning"
	@ echo "$(indent)the expected values). You can use it as a template for your own"
	@ echo "$(indent)definition. Don't edit it directly or you may accidentally loose"
	@ echo "$(indent)your changes from 'make' updating the file."
	@ echo ""
	$(genWrappers) $(wrappersDefTempl) $(wrappersDir)
	chmod u+x $(wrappersDir)/*
endif

.PHONY: template
template: $(wrappersDefTempl)

.PHONY: analysis
analysis: $(testpTsv) $(extras)

$(wrappersDefTempl): $(testpCmdTsv) $(genTemplate)
	$(genTemplate) $< $@

# jsonToTsv(tsvTableNameVar, tabularJsonNameVar, datConverionJqScript)
define jsonToTsv
$$($(1)): $$($(2))
	$(jsonToCsv) --sep '	' --quotechar "'" $$($(2)) $$@

$$($(2)): $(DAT) $(testpJq) $$($(3))
ifdef DAT
	cat $(DAT)         \
	    | jq -f $(testpJq) \
	    | jq -f $$($(3)) > $$@
else
	$(noDATerrMsg)
endif
endef

$(eval $(call jsonToTsv,testpTsv,datTabJson,tabTestpJq))
$(eval $(call jsonToTsv,testpCmdTsv,testpTabJson,tabTestpCmdJq))

$(allDevices): $(DAT)
ifdef DAT
	cat $(DAT) | jq 'keys' > $@
else
	$(noDATerrMsg)
endif

$(allAccPropsList): $(DAT)
ifdef DAT
	cat $(DAT) | jq '[ .. | .accessor? | objects | keys | .[] ] | unique' > $@
else
	$(noDATerrMsg)
endif

$(allAccTypesList): $(DAT) $(testpJq)
ifdef DAT
	cat $(DAT) | jq -f $(testpJq) | jq '[.[] | .accessor.type] | unique' > $@
else
	$(noDATerrMsg)
endif


$(wrappersDir):
	mkdir -p $@

# Should be called before clean
$(foreach target,  \
    $(stdTargets), \
    $(eval $(call standardTarget,$(target))))

.PHONY: clean
clean: 
	rm -rf $(toClean)

.PHONY: list
list:
	@LC_ALL=C $(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | awk -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'


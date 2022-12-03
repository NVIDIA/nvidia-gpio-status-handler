indent := "    "
csvSep := ";"
# csvSep := $(subst x,	,x)
csvQuote := "'"

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
srcDir           := src
testpJq          := $(srcDir)/jq/testpoints.jq
tabTestpJq       := $(srcDir)/jq/tabular-testpoints.jq
tabTestpCmdJq    := $(srcDir)/jq/tabular-testpoints-CMDLINE.jq
commandsJq       := $(srcDir)/jq/commands-list.jq
genWrappers      := $(srcDir)/python/generate-wrappers.py
genTemplate      := $(srcDir)/python/make-wrappers-mockup-definition-template.py
jsonToCsv        := $(srcDir)/python/json-to-csv.py
genWrapsCat      := $(srcDir)/python/concat-perf-tests.py
genWrapsAgg      := $(srcDir)/python/agg-perf-tests.py
mockupDeployProg := $(srcDir)/bash/deployment-script.sh

csvOpts := --sep $(csvSep) --quotechar $(csvQuote)
jsonToCsvCmd := $(jsonToCsv) $(csvOpts)

# Input
# DAT
wrappersDefTempl := wrappers-mockup-definition-template.tsv
stdTargets += $(wrappersDefTempl)

# Intermediaries and outputs
outDir      := data
toClean += $(outDir)

testpDir     := $(outDir)/testpoints
testpCmdDir  := $(outDir)/testpoints-CMDLINE
extraDir     := $(outDir)/extra

datTabJson   := $(testpDir)/tabular-testpoints.json
testpTabJson := $(testpCmdDir)/tabular-testpoints-CMDLINE.json

testpTsv     := $(testpDir)/testpoints.tsv
testpCmdTsv  := $(testpCmdDir)/testpoints-CMDLINE.tsv

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

# Deployment packages #########################################################

# Mockup wrappers package

mockupsDeployDir := wrapper-mockups-deploy-pkg
toClean += $(mockupsDeployDir)

wrappersDir             := $(mockupsDeployDir)/wrapper-mockups
mockupsDeployTargetProg := $(mockupsDeployDir)/deploy.sh
mockupsDeployPkg        := $(mockupsDeployDir).tgz

stdTargets += $(mockupsDeployTargetProg) $(mockupsDeployPkg)

# Performance testing package

perfDeployDir := performance-test-deploy-pkg

testpCmdList := $(perfDeployDir)/testpoint-commands.list
stdTargets += $(testpCmdList)

perfDeployProg := $(perfDeployDir)/wrappers-exec-time.sh

perfDeployPkg := $(perfDeployDir).tgz
stdTargets += $(perfDeployPkg)

perfRunProg := $(perfDeployDir)/run.sh
stdTargets += $(perfRunProg)

perfTestResultCsv := $(perfDeployDir)/results.csv

# Performance testing #########################################################

perfTestResDir     := performance-test-results
perfTestResUnitDir := $(perfTestResDir)/unitary
perfTestResAggDir  := $(perfTestResDir)/aggregate

wrapRunsAggCsv := $(perfTestResAggDir)/aggregated.csv
wrapRunsAllCsv := $(perfTestResAggDir)/all.csv
perfStatsCsv   := $(perfTestResAggDir)/performance-stats.csv
nonWorkingCsv  := $(perfTestResAggDir)/non-working-wrappers.csv
stdTargets += $(wrapRunsAllCsv)

# Rules #######################################################################

.PHONY: all
all: datDependent datFree

.PHONY: datDependent
datDependent: wrappers template analysis $(perfDeployPkg) $(mockupsDeployPkg)

.PHONY: datFree
datFree: .gitignore wrapperRunsResults

.gitignore:
	@ echo "$(outDir)/*" > $@
	@ echo "$(mockupsDeployDir)/*" >> $@
	@ echo "$(mockupsDeployPkg)" >> $@
	@ echo "$(wrappersDefTempl)" >> $@
	@ echo "$(testpCmdList)" >> $@
	@ echo "$(perfRunProg)" >> $@
	@ echo "$(perfDeployPkg)" >> $@
stdTargets += .gitignore

$(perfDeployPkg): $(perfDeployProg) $(testpCmdList) $(perfRunProg) $(perfDeployDir)
	tar --dereference --create --file $@ --auto-compress $(perfDeployDir)
	@ echo "$(indent)"
	@ echo "$(indent)Package \`$@' created. Copy it to OpenBMC, then, inside:"
	@ echo "$(indent)"
	@ echo "$(indent)    tar -xf $@"
	@ echo "$(indent)    $(perfRunProg) [repeat-count]"
	@ echo "$(indent)"
	@ echo "$(indent)The results will be saved in \`$(perfTestResultCsv)'"
	@ echo "$(indent)"

define perfRunProgContent
#!/usr/bin/env bash
scrFullpath=$$(realpath $$0)
scrDir=$$(dirname $${scrFullpath})
echo "Running commands from '$${scrDir}/$(call notdir,$(testpCmdList))'"
echo "Writing results to '$${scrDir}/$(call notdir,$(perfTestResultCsv))'"
$${scrDir}/$(call notdir,$(perfDeployProg)) --sep $(csvSep) \\
    $${scrDir}/$(call notdir,$(testpCmdList)) \\
    $${scrDir}/$(call notdir,$(perfTestResultCsv)) $$1
endef

export perfRunProgContent
$(perfRunProg):
	echo "$$perfRunProgContent" > $@
	chmod u+x $@

$(mockupsDeployPkg): deployTree alwaysUpdate
	tar --dereference --create --file $(mockupsDeployPkg) --auto-compress $(mockupsDeployDir)
	@ echo "$(indent)"
	@ echo "$(indent)Package '$(mockupsDeployPkg)' created. Copy it to OpenBMC, then, inside:"
	@ echo "$(indent)"
	@ echo "$(indent)    tar -xf $(mockupsDeployPkg)"
	@ echo "$(indent)    $(mockupsDeployTargetProg)"
	@ echo "$(indent)"
	@ echo "$(indent)It will substitute the mockup wrappers for the original ones. Run"
	@ echo "$(indent)"
	@ echo "$(indent)    $(mockupsDeployTargetProg) original"
	@ echo "$(indent)"
	@ echo "$(indent)to return to the original wrappers"
	@ echo "$(indent)"
stdTarget += $(mockupsDeployPkg)

.PHONY: deployTree
deployTree: $(mockupsDeployTargetProg) wrappers

# phony target will always trigger the update
.PHONY: alwaysUpdate
alwaysUpdate:

$(mockupsDeployTargetProg): | $(mockupDeployProg)
	ln --force --symbolic --relative $(mockupDeployProg) $@
	chmod u+x $@

# # ###########################################################################

.PHONY: wrappers
ifdef WRAPPERS_DEF
wrappers: $(genWrappers) $(WRAPPERS_DEF) | $(wrappersDir)
	$(genWrappers) $(csvOpts) $(WRAPPERS_DEF) $(wrappersDir)
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
	$(genWrappers) $(csvOpts) $(wrappersDefTempl) $(wrappersDir)
	chmod u+x $(wrappersDir)/*
endif

.PHONY: template
template: $(wrappersDefTempl)

.PHONY: analysis
analysis: $(testpTsv) $(extras)

$(wrappersDefTempl): $(testpCmdTsv) $(genTemplate)
	$(genTemplate) $(csvOpts) $< $@

# jsonToTsv(tsvTableNameVar, tabularJsonNameVar, datConverionJqScript)
define jsonToTsv
$$($(1)): $$($(2))
	$(jsonToCsvCmd) $$($(2)) $$@

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

$(testpCmdList): $(testpTabJson) $(commandsJq)
	cat $(testpTabJson) | jq -f $(commandsJq) | $(jsonToCsvCmd) --no_header > $@

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

# Data aggregation ############################################################

.PHONY: wrapperRunsResults
wrapperRunsResults: $(wrapRunsAllCsv) $(perfStatsCsv) $(nonWorkingCsv)

# `alwaysUpdate' because there is no way track changes to the
# `perfTestResUnitDir' directory as a "data set" (so that if any
# new file appeared there, or some of them was changed, the
# update was appropriately triggered)
$(wrapRunsAllCsv): $(genWrapsCat) alwaysUpdate | $(perfTestResUnitDir)
	python $(genWrapsCat) $(csvOpts) $(perfTestResUnitDir) $@

$(perfStatsCsv) $(nonWorkingCsv): $(wrapRunsAllCsv) $(genWrapsAgg) \
		| $(perfTestResAggDir)
	python $(genWrapsAgg) $(csvOpts) $(wrapRunsAllCsv) \
	    $(nonWorkingCsv) $(perfStatsCsv)
stdTargets += $(nonWorkingCsv) $(perfStatsCsv)

$(perfTestResUnitDir):
	mkdir -p $@

# Technical targets ###########################################################

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


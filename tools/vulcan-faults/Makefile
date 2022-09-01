this := Makefile

# Generalized vulcan faults script ############################################

dataInDir := data/in
dataOutDir := data/out

vfName := vulcan-faults
vfSummaryTblName := $(vfName)-summary
vfOpCodeTblName := $(vfName)-smbpbi-op-code

jsonDeduceScr := event-info-skeleton.xsh

# Inputs ######################################################################

vfOpCodeRawTsv := $(dataInDir)/$(vfOpCodeTblName)-raw.tsv
vfSummaryTsv := $(dataInDir)/$(vfSummaryTblName).csv

vfOpCodeTsv := $(dataOutDir)/$(vfOpCodeTblName).tsv
#eventInfoFlatJson := $(dataOutDir)/event-info-flat.json
eventInfoFlatJson := $(dataOutDir)/event_info.json

define vfOpCodeHelp
The '$(vfOpCodeRawTsv)' file needs to be created manually.
1. Go to:
   https://docs.google.com/spreadsheets/d/16h1BEXKQqtLCMwn_Zrve44vlgve4BtzPd2wldudYsjs/edit#gid=2096320419
2. ⟦File → Download → tsv⟧: Save as '$(vfOpCodeRawTsv)'
3. Re-run make
endef
export vfOpCodeHelp

define vfSummaryTsvHelp
The '$(vfSummaryTsv)' file needs to be created manually.
1. Go to:
   https://docs.google.com/spreadsheets/d/1OzXlCVPVFHe_CxohOV1J7cU6p_GAZEHJZyknDEsTUGU/edit#gid=1914097812
2. [File -> Download -> ods]
3. Open the saved file in LibreOffice
4. Switch to "Summary" tab
5. [File -> Save a copy -> Text CSV (.csv)]: '$(vfSummaryTsv)'
   1. Character Set: "Unicode (UTF-8)"
   2. Field delimiter: "{Tab}"
   3. String delimiter: "'"
   4. [x] Save cell contents as shown
6. Re-run 'make'
endef
export vfSummaryTsvHelp

###############################################################################
#                                    Rules                                    #
###############################################################################

$(eventInfoFlatJson):           \
		$(vfSummaryTsv) \
		$(vfOpCodeTsv)  \
		$(jsonDeduceScr) \
                $(this)
	./$(jsonDeduceScr)               \
	    $(vfSummaryTsv)              \
	    $(vfOpCodeTsv)               \
	    $(eventInfoFlatJson)

$(vfOpCodeTsv): $(vfOpCodeRawTsv) remove-cr.sh $(this)
	./remove-cr.sh $< | head -n 48 > $@

$(vfOpCodeRawTsv):
	@ echo "$$vfOpCodeHelp"
	exit 1

$(vfSummaryTsv):
	@ echo "$$vfSummaryTsvHelp"
	exit 1


.PHONY: clean
clean:
	rm -rf $(vfOpCodeTsv) $(eventInfoFlatJson)

.PHONY: CLEAN
CLEAN: clean
	rm -rf $(vfOpCodeRawTsv) $(vfSummaryTsv)

.PHONY: list
list:
	@LC_ALL=C $(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | awk -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'

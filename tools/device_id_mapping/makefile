DAT_DEFAULT = ../../examples/dat.json
ifndef DAT
    datNotProvided := 1
    DAT = $(DAT_DEFAULT)
endif

devAddIdsList := devices-add-ids.list
devDatIdsList := devices-dat-ids.list
devNewIdsList := devices-new-ids.list
devOldIdsList := devices-old-ids.list
devAllIdsList := devices-all-ids.list

devIdsLookup := device_id_map.csv

.PHONY: all
all: .gitignore $(devIdsLookup)

$(devNewIdsList): $(devAddIdsList) $(devDatIdsList)
	cat $^ > $@

$(devDatIdsList): $(DAT)
ifdef datNotProvided
	@ echo "WARNING: 'DAT' variable not defined."\
	    "Using '$(DAT_DEFAULT)' which may be outdated"
endif
	cat $(DAT) | jq 'keys | .[]' | sed --quiet --e 's/"\(.*\)"/\1/p' > $@

$(devOldIdsList): $(devNewIdsList)
	./gen-old-ids.sh $< $@

$(devAllIdsList): $(devOldIdsList) $(devNewIdsList)
	cat $^ | sort | uniq > $@

$(devIdsLookup): $(devAllIdsList)
	./device_id_map_gen.sh './device-id-norm-gen.sh --ext' $(devAllIdsList) > $@; exit 0

ignoreList := $(devDatIdsList) $(devNewIdsList) $(devOldIdsList) $(devAllIdsList) \
                .gitignore

.gitignore:
	@ echo "$(ignoreList)" | sed --expression='s/ /\n/g' > $@

cleanList := $(ignoreList) $(devIdsLookup)

.PHONY: clean
clean:
	rm -rf $(cleanList)

.PHONY: list
list:
	@LC_ALL=C $(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | awk -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'

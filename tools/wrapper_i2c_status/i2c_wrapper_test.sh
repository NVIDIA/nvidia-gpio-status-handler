#!/usr/bin/env bash

testsTotal=0
testsPassed=0
testsFailed=0

scriptName="i2c_wrapper"
# export for dependant map tool, you might need to set it yourself if doesn't work
export DEVICE_MAP_FILE="../device_id_mapping/device_id_map.csv"
export DEVICE_NAME_MAPPER="../device_id_mapping/device-id-norm.sh"

perfTest() 
{
    testsTotal=$(($testsTotal + 1))
    local symbol="$1"
    local dev="$2"
    local artOut="$3"
    local rcCode=$4
    local exp="$5"

    # workaround to bash quotes stripping
    artOut=${artOut//' '/'_'}

    local cmd="./$scriptName -test-run $symbol $dev $artOut $rcCode"
    local resp=$($cmd);

    # workaround - do second quiet run just to catch rc code, above always rets 0
    $cmd 1> /dev/null 2> /dev/null
    local cmdRc=$?

    if [ "$resp" != "$exp" ] || [ $rcCode -ne $cmdRc ]; then
        testsFailed=$(($testsFailed + 1))
        echo "ERROR: test failed $symbol $dev $artOut $rcCode $exp"

        if [ "$resp" != "$exp" ]; then
            echo "resp $resp != expected $exp"
        fi

        if [ $rcCode -ne $cmdRc ]; then
            echo "resp RC $cmdRc != expected RC $rcCode"
        fi

    else
        testsPassed=$(($testsPassed + 1))
    fi

    echo -n -e "test $testsTotal\r"
}

summary()
{
    echo "Tests total $testsTotal passed $testsPassed failed $testsFailed"
    if [ $testsFailed -ne 0 ]; then
        exit 1
    fi
    exit 0
}

# quick qemu test
# ./i2c_wrapper -verbose -dry-run i2c_access GPU_SXM_4; echo "rc = $?"
# ./i2c_wrapper -verbose -dry-run i2c_access GPU_SXM_11; echo "rc = $?"

###########################################################
# bmc uses different method (detect instead of get)
perfTest "i2c_access" "BMC_0" "<dummy>" 0 $'link-up'
perfTest "i2c_access" "BMC_0" "<dummy>" 1 $'link-down; rc 1'

perfTest "i2c_access" "GPU_SXM_4" "0x11" 0 $'link-up'
perfTest "i2c_access" "GPU_SXM_0" " " 1 "Error: cannot map deviceID (GPU_SXM_0)"
perfTest "i2c_access" "GPU_SXM_4" "Error: Read failed" 1 $'link-down; rc 1'

perfTest "i2c_access" "GPU_SXM_1"      "0x11" 0 $'link-up'
perfTest "i2c_access" "GPU_SXM_2"      "0x11" 0 $'link-up'
perfTest "i2c_access" "GPU_SXM_3"      "0x11" 0 $'link-up'
perfTest "i2c_access" "GPU_SXM_4"      "0x11" 0 $'link-up'
perfTest "i2c_access" "GPU_SXM_5"      "0x11" 0 $'link-up'
perfTest "i2c_access" "GPU_SXM_6"      "0x11" 0 $'link-up'
perfTest "i2c_access" "GPU_SXM_7"      "0x11" 0 $'link-up'
perfTest "i2c_access" "GPU_SXM_8"      "0x11" 0 $'link-up'
perfTest "i2c_access" "NVSwitch_0"     "0x11" 0 $'link-up'
perfTest "i2c_access" "NVSwitch_1"     "0x11" 0 $'link-up'
perfTest "i2c_access" "NVSwitch_2"     "0x11" 0 $'link-up'
perfTest "i2c_access" "NVSwitch_3"     "0x11" 0 $'link-up'
perfTest "i2c_access" "PCIeRetimer_0"  "0x11" 0 $'link-up'
perfTest "i2c_access" "PCIeRetimer_1"  "0x11" 0 $'link-up'
perfTest "i2c_access" "PCIeRetimer_2"  "0x11" 0 $'link-up'
perfTest "i2c_access" "PCIeRetimer_3"  "0x11" 0 $'link-up'
perfTest "i2c_access" "PCIeRetimer_4"  "0x11" 0 $'link-up'
perfTest "i2c_access" "PCIeRetimer_5"  "0x11" 0 $'link-up'
perfTest "i2c_access" "PCIeRetimer_6"  "0x11" 0 $'link-up'
perfTest "i2c_access" "PCIeRetimer_7"  "0x11" 0 $'link-up'
perfTest "i2c_access" "PCIeSwitch_0"   "0x11" 0 $'link-up'
perfTest "i2c_access" "FPGA_0"         "0x11" 0 $'link-up'
perfTest "i2c_access" "BMC_0"          "0x11" 0 $'link-up'

###########################################################

summary

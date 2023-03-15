dev_phase=true
config_gen_branch="device_id_lang_json_gen"
dev_aml_branch="selftest_event_categories"
event_injector_refresh_branch="dev/prep_deploy_deps"
sdk_path="<your_sdk_path>/sdk/environment-setup-armv7ahf-vfpv4d16-openbmc-linux-gnueabi"

wrapper_mockup_model=true #use dev version of wrapper mockups

generate_configs () {
    cd $start_dir
    echo "Generating configs..."
    if $dev_phase; then
        echo "dev phase configured, checking out onto $config_gen_branch branch"
        git checkout $config_gen_branch
        rc=$?
        if [[ $rc -ne 0 ]]; then
            echo "wrong branch chosen, tried to checkout $config_gen_branch"
            return 1
        fi
        git fetch && git pull
    fi
    cd tools/configs
    make clean >> "$deploy_dir_path/make_configs.log"
    make configs >> "$deploy_dir_path/make_configs.log"
    rc=$?
    if [[ $rc -ne 0 ]]; then
        echo "make configs error, you might need to install python module graph-theory"
        echo "check https://pypi.org/project/graph-theory/"
        return 1
    fi
    cp to-drop-in-openbmc-repo/dat.json $deploy_dir_path
    cp to-drop-in-openbmc-repo/event_info.json $deploy_dir_path
    dat_path=$(realpath $deploy_dir_path/dat.json)
    touch dat_md5_$(md5sum $dat_path | cut -d ' ' -f 1)
    event_info_path=$(realpath $deploy_dir_path/event_info.json)
    echo "created dat path = $dat_path"
    echo "created event_info path = $event_info_path"
    return 0
}


generate_wrapper_mockups () {
    cd $start_dir
    echo "Generating wrapper mockups..."
    if $dev_phase; then
        echo "dev phase configured, checking out onto $config_gen_branch branch"
        git checkout $config_gen_branch
        rc=$?
        if [[ $rc -ne 0 ]]; then
            echo "wrong branch chosen, tried to checkout $config_gen_branch"
            return 1
        fi
        git fetch && git pull
    fi

    cd tools/configs
    make clean >> "$deploy_dir_path/make_wrapper_mockups.log"

    if $wrapper_mockup_model; then
        make deployPkg >> "$deploy_dir_path/make_wrapper_mockups.log"
        rc=$?
        cp deploy-pkg.tgz  $deploy_dir_path
    else
        make DAT=$dat_path >> "$deploy_dir_path/make_wrapper_mockups.log"
        rc=$?
        cp wrapper-mockups-deploy-pkg.tgz $deploy_dir_path
    fi

    if [[ $rc -ne 0 ]]; then
        return 1
    fi

# temporary regression, doesnt build
    # make envSnapshotScr
    # rc=$?
    # if [[ $rc -ne 0 ]]; then
    #     echo "error during envSnapshotScr creation, maybe you need to install shfmt
    #     sudo snap install shfmt"
    #     return 1
    # fi
    # cp  env_snapshot.sh $deploy_dir_path

    return 0
}


build_aml_binaries () {
    cd $start_dir
    echo "Building AML binaries..."
    if $dev_phase; then
        echo "dev phase configured, checking out onto $dev_aml_branch branch"
        git checkout $dev_aml_branch
        rc=$?
        if [[ $rc -ne 0 ]]; then
            echo "wrong branch chosen, tried to checkout $dev_aml_branch"
            return 1
        fi
        git fetch && git pull
    fi
    echo "provide SDK path to enviroment setup file to be sourced or"
    echo "leave empty to use default: $sdk_path"
    read sdk_path_prompt
    if [ -n "$sdk_path_prompt" ] && [ "$sdk_path_prompt" != "$sdk_path" ]; then
        sdk_path=$sdk_path_prompt
        echo "provided new sdk path = $sdk_path"
    fi
    echo "starting..."
    source $sdk_path
    build_dir="build_deploy_pkg"
    rm $build_dir -r
    meson $build_dir
    ninja -C $build_dir

    rc=$?
    if [[ $rc -ne 0 ]]; then
        return 1
    fi

    arm-openbmc-linux-gnueabi-strip $build_dir/tools/selftest_tool
    arm-openbmc-linux-gnueabi-strip $build_dir/src/oobamld
    arm-openbmc-linux-gnueabi-strip $build_dir/src/lib*

    cp $build_dir/tools/selftest_tool $deploy_dir_path
    cp $build_dir/src/oobamld $deploy_dir_path
    cp $build_dir/src/lib* $deploy_dir_path
    return 0
}


include_sw_error_injector () {
    # cd $start_dir
    #refresh event_injections.sh from deploy package
    # if $dev_phase; then
    #     echo "dev phase configured, checking out onto $event_injector_refresh_branch branch"
    #     git checkout $event_injector_refresh_branch
    #     rc=$?
    #     if [[ $rc -ne 0 ]]; then
    #         echo "wrong branch chosen, tried to checkout $event_injector_refresh_branch"
    #         return 1
    #     fi
    #     git fetch && git pull
    # fi

    cd $deploy_dir_path
    git clone https://gitlab-master.nvidia.com/dgx/bmc/nvidia-oobaml.git
    rc=$?
    if [[ $rc -ne 0 ]]; then
        return 1
    fi
    mv nvidia-oobaml nvidia-oobaml-master
    cd nvidia-oobaml-master
    git checkout EInj_environment_control
    rc=$?
    if [[ $rc -ne 0 ]]; then
        return 1
    fi
    cd ..

    echo "generating fresh event_injector.bash"
    python3 nvidia-oobaml-master/tools/event_validation.py --json $event_info_path --generate-script-only
    rc=$?
    if [[ $rc -ne 0 ]]; then
        echo "error generating event_injector.bash"
        return 1
    fi
    rm nvidia-oobaml-master -rf

    echo "downloading deploy package"
    local einj_deploy_package_name="HMC_SW_EInj_Injector.tar.gz"
    local tmp_deploy_repack_dir="tmp_deploy_repack_dir"
    wget https://gitlab-master.nvidia.com/cmazieri/openbmc-dbus-populate-tool/-/raw/main/deploy/$einj_deploy_package_name
    rc=$?
    if [[ $rc -ne 0 ]]; then
        echo "error downloading einj deploy package"
        return 1
    fi

    #unpack, update script, repack again
    echo "repacking deploy package with fresh event_injector.bash"
    mkdir $tmp_deploy_repack_dir
    tar zxf $einj_deploy_package_name -C $tmp_deploy_repack_dir
    cd $tmp_deploy_repack_dir
    rm oobaml/bin/event_injector.bash
    cp /tmp/event_injector.bash oobaml/bin/
    touch event_injection_sh_updated=yes
    tar -zcvf ../$einj_deploy_package_name *
    cd ..
    rm $tmp_deploy_repack_dir -r
    
    return 0
}


include_run_scripts () {
    local do_start='
/tmp/test_AML.sh start --logs --devel --start-with-clean-environment
cd /tmp/
echo "now you may want to \"source ./inject-event-lib.sh\" for your current shell manually if needed"
'

    local run_selftest_tool='
source /tmp/inject-event-lib.sh
clean_selftest_all
LD_LIBRARY_PATH=/tmp:/tmp/oobaml/bin
export LD_LIBRARY_PATH
PATH=/tmp/oobaml/bin:$PATH
export PATH
PATH=/tmp/oobaml/bin:$PATH LD_LIBRARY_PATH=/tmp:/tmp/oobaml/bin ./selftest_tool -d /tmp/dat.json -r /tmp/selftest_report.json
'

    local run_aml_manually='
killall oobamld
source /tmp/inject-event-lib.sh
clean_selftest_all
LD_LIBRARY_PATH=/tmp:/tmp/oobaml/bin
export LD_LIBRARY_PATH
PATH=/tmp/oobaml/bin:$PATH
export PATH
PATH=/tmp/oobaml/bin:$PATH LD_LIBRARY_PATH=/tmp:/tmp/oobaml/bin /tmp/oobamld -l 3 -e /tmp/event_info.json -d /tmp/dat.json
'

    local cheatsheet='
LD_LIBRARY_PATH=/tmp:/tmp/oobaml/bin ./oobamld --diagnostics-mode --dat path/to/your/dat.json --event-info path/to/your/event_info.json > results.json
'

    local test_wrapper_overlay="
echo \"no overlay value\"
rm /tmp/oobaml/bin/overlay-wrapper-mockups-profile.csv
/tmp/oobaml/bin/mctp-vdm-util-wrapper active_auth_status GPU_SXM_1
echo \"adding overlay, expected value 6\"
echo \"mctp-vdm-util-wrapper;active_auth_status GPU_SXM_1;6;0\" > /tmp/oobaml/bin/overlay-wrapper-mockups-profile.csv
/tmp/oobaml/bin/mctp-vdm-util-wrapper active_auth_status GPU_SXM_1"

#     local test_inj="
# echo \"mctp-vdm-util-wrapper;active_auth_status GPU_SXM_1;6;0\" > /tmp/oobaml/bin/overlay-wrapper-mockups-profile.csv
# /tmp/test_AML.sh inject --event \"Secure boot failure\" --device GPU --device-index 1"

    local test_inj="
#echo \"mctp-vdm-util-wrapper;active_auth_status GPU_SXM_1;6;0\" > /tmp/oobaml/bin/overlay-wrapper-mockups-profile.csv
#LOGS=debug /tmp/inject-event.sh  GPU  1  \"Secure boot failure\"
LOGS=debug /tmp/inject-event.sh  GPU  1  \"OverTemp\" "

    local do_deploy=""
    if $wrapper_mockup_model; then
        do_deploy="tar zxvf HMC_SW_EInj_Injector.tar.gz -C /tmp/
tar zxvf deploy-pkg.tgz
deploy-pkg/deploy.sh mockup /tmp/oobaml/bin
cp *.json /tmp/
cp lib* /tmp/
cp oobamld /tmp/
cp selftest_tool /tmp/
echo \"Done - deployed configs and binaries and wrapper mocks to /tmp/\""
    else
        do_deploy="tar zxvf HMC_SW_EInj_Injector.tar.gz
cd oobaml
/bin/bash scenario.sh
cd ..
tar zxvf wrapper-mockups-deploy-pkg.tgz
wrapper-mockups-deploy-pkg/deploy.sh"
    fi

    echo "$do_start" > $deploy_dir_path/zzz_do_start_aml.sh
    chmod +x $deploy_dir_path/zzz_do_start_aml.sh
    echo "$do_deploy" > $deploy_dir_path/zzz_do_deploy_aml.sh
    chmod +x $deploy_dir_path/zzz_do_deploy_aml.sh
    echo "$run_selftest_tool" > $deploy_dir_path/zzz_run_selftest_tool.sh
    chmod +x $deploy_dir_path/zzz_run_selftest_tool.sh
    echo "$run_aml_manually" > $deploy_dir_path/zzz_run_aml_manually.sh
    chmod +x $deploy_dir_path/zzz_run_aml_manually.sh
    echo "$test_wrapper_overlay" > $deploy_dir_path/zzz_test_wrapper_overlay.sh
    chmod +x $deploy_dir_path/zzz_test_wrapper_overlay.sh
    echo "$test_inj" > $deploy_dir_path/zzz_test_inj.sh
    chmod +x $deploy_dir_path/zzz_test_inj.sh
    echo "$cheatsheet" > $deploy_dir_path/zzz_cheatsheet.sh
    chmod +x $deploy_dir_path/zzz_cheatsheet.sh
    return 0
}

package_deploy () {
    cd $start_dir
    tar zcvf $deploy_dir.tar.gz -C $(dirname $deploy_dir_path) $(basename $deploy_dir_path)
    rc=$?
    if [[ $rc -ne 0 ]]; then
        return 1
    fi
    tar_deploy_package_path=$(realpath $deploy_dir.tar.gz)
    return 0
}

echo "Purpose of this script is to repeatably generate single package out of 
freshest sources that can be deployed into qemu. Its main usage is to tinker
with AML and speed up environment setup during work on manual validation
instruction."
echo "It wraps integrations of several branches, tools, packages and turns them
into all in one. Can be used as a reference to generate them manually."
echo "Eg.   (1) ./prep_deploy.sh"
echo "      (2) scp -P 2222 deploy-2023-02-21-00-02-18.tar.gz root@127.0.0.1:/tmp/"
echo "(qemu)(3) tar zxvf deploy-2023-02-21-00-02-18.tar.gz"
echo "(qemu)(4) cd deploy-2023-02-21-00-02-18"
echo "(qemu)(5) ./zzz_do_deploy.sh" 
echo "(qemu)(6) ./zzz_do_start.sh"
echo "======================================================================"
echo "WARNING! This script and integrated tools are still under development. 
Currently it checkouts branches, fetches and pulls changes. 
Its advised to backup uncommited changes."
echo "Confirm to continue [y/n]"
echo "======================================================================"
read user_confirmation
user_confirmation="${user_confirmation,,}"

if [[ "$user_confirmation" != "y" ]]; then
    echo "aborted by user"
    exit 1
fi

echo "starting..."
printf -v date '%(%Y-%m-%d-%H-%M-%S)T' -1
deploy_dir="deploy-$date"
mkdir $deploy_dir
deploy_dir_path=$(realpath $deploy_dir)
start_dir=$PWD
echo "created $deploy_dir_path"

generate_configs
rc=$?
if [[ $rc -ne 0 ]]; then
    echo "failed to generate configs, check stdout for errors"
    exit 1
fi

generate_wrapper_mockups
rc=$?
if [[ $rc -ne 0 ]]; then
    echo "failed to generate wrapper mockups, check stdout for errors"
    exit 1
fi

include_sw_error_injector
rc=$?
if [[ $rc -ne 0 ]]; then
    echo "error cannot download sw einj deploy package"
    exit 1
fi

#this should be last, it sources arm sdk and messes up env
build_aml_binaries
rc=$?
if [[ $rc -ne 0 ]]; then
    echo "failed to generate aml binaries, check stdout for errors"
    exit 1
fi

include_run_scripts
rc=$?
if [[ $rc -ne 0 ]]; then
    echo "error cannot include run scripts"
    exit 1
fi

tar_deploy_package_path=""
package_deploy
rc=$?
if [[ $rc -ne 0 ]]; then
    echo "error during packaging deploy"
    exit 1
fi

echo "Success! Ready: $tar_deploy_package_path"
exit 0

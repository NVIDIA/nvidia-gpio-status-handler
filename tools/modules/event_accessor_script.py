"""
Module for handling devices from Json input file which as 'accessor' information not empty
So far accessor.type = DBUS are handled
"""

import com ## common constants and and functions

from injector_script_base import InjectorScriptBase

class EventAccessorInjectorScript(InjectorScriptBase):
    """
    This class parses a Json Event file, then:
       1 Generates a bash script with busctl commands
    """

    def __init__(self, json_file):
        super().__init__(json_file)
        self._busctl_cmd_counter = 0
        self._accessor_dbus_expected_events_list = []
        """
        Keeps commands in a list just to have the total counter
        Then comands are generated later
        """
        self._commands_information_list = []


    def create_script_file(self):
        """
        override parent method
        """
        super().priv_create_script_file("#!/bin/bash\n"
            "# This script is generated by event_validation.py,"
            " be aware that changes will be lost!\n\n"
            "SERVICE=xyz.openbmc_project.GpuMgr\n"
            "\n"
            "## global variables\n"
            "gl_property_value=\"\"\n"
            "gl_property_type=\"\"\n"
            "gl_new_property_value=\"\"\n"
            "gl_old_property_value=\"\"\n"
            "gl_rc=0\n"
            "gl_script_rc=0\n"
            "gl_total_comands=0  # will be set as the total intended commands\n"
            "gl_counter_commands=0 # counter for commands being executed\n"
            "gl_injections=0  # successful injections\n"
            "gl_current_logging_entries=0"
            "\n"
            "gl_debug=1\n"
            "gl_error_file=/tmp/_event_error_$$\n"
            "DRY_RUN=1 # default do not run commands 'busctl set-property'\n"
            "\n"
            "while [ \"$1\" != \"\" ]; do\n"
            "   [ \"$1\" = \"-r\" -o \"$1\" = \"--run\" ] && DRY_RUN=0\n"
            "\n"
            "   [ \"$1\" = \"-d\" -o \"$1\" = \"--debug\" ] && gl_debug=1\n"
            "   shift\n"
            "done\n"
            "\n"
            "if [ $DRY_RUN -eq 1 ]; then\n"
            "   echo; echo running in DRY-RUN mode...; echo ...\n"
            "fi\n"
            "\n"
            "latest_looging_entry()\n"
            "{\n"
            "   [ $gl_debug -eq 0 ] && return 0 # only when debug is active\n"
            "   latest_entry=0\n"
            "   for entry_value in  $(busctl tree xyz.openbmc_project.Logging | "
            "grep /xyz/openbmc_project/logging/entry/ | awk -F / '{ print $NF}' 2> $gl_error_file)\n"
            "   do\n"
            "      entry=${entry_value//[!0-9]/}\n"
            "      [ \"$entry\" != \"\" -a $entry -gt $latest_entry ] && latest_entry=$entry\n"
            "   done\n"
            "   echo $latest_entry\n"
            "}\n"
            "\n"
            "\n"
            "property_get() #1=object_path $2=interface, $3=property_name\n"
            "{\n"
            "    gl_property_value=\"\"\n"
            "    gl_property_type=\"\"\n"
            "    local cmd=\"busctl get-property $SERVICE $@\"\n"
            "    echo \"property_get(): $cmd\"\n"
            "    local get_value=$(eval $cmd 2> $gl_error_file)\n"
            "    if [ $? -ne 0 -o \"$get_value\" = \"\" ]; then\n"
            "        gl_rc=1\n"
            "    else\n"
            "        echo \" $get_value\"\n"
            "        gl_property_value=$(echo $get_value | awk '{print $NF}' 2> $gl_error_file )\n"
            "        gl_property_type=$(echo $get_value | awk '{print $(NF-1)}' 2> $gl_error_file)\n"
            "        [ \"$gl_property_value\" = \"\" -o \"$gl_property_type\" = \"\" ] && gl_rc=1\n"
            "    fi\n"
            "    return $gl_rc\n"
            "}\n"
            "\n"
            "\n"
            "change_value_method() #1=value to change\n"
            "{\n"
            "    result=0\n"
            "    case \"$METHOD\" in \n"
            "       \"add\")     gl_new_property_value=$[ ${1} + ${METHOD_VALUE} ];;\n"
            "       \"bitmask\") gl_new_property_value=$[ ( ${1} + 1 ) | ${METHOD_VALUE} ];;\n"
            "       \"lookup\")  gl_new_property_value=${METHOD_VALUE};;\n"
            "    esac\n"
            "}\n"
            "\n"
            "\n"
            "property_set_method() #1=object_path $2=interface, $3=property_name\n"
            "{\n"
            "    gl_new_property_value=\"\"\n"
            "    gl_old_property_value=$gl_property_value\n"
            "    if [ \"$METHOD\" = \"lookup\" ]\n"
            "    then\n"
            "        local save_method_value=${METHOD_VALUE}\n"
            "        METHOD=\"add\" METHOD_VALUE=\"1\" property_set \"$@\"\n"
            "        gl_rc=$?\n"
            "        gl_property_value=$gl_new_property_value\n"
            "        METHOD=\"lookup\"\n"
            "        METHOD_VALUE=\"$save_method_value\"\n"
            "    fi\n"
            "    if [ $gl_rc -eq 0 ]\n"
            "    then\n"
            "        property_set \"$@\"\n"
            "    fi\n"
            "    return $gl_rc\n"
            "}\n"
            "\n"
            "\n"
            "property_set() #1=object_path $2=interface, $3=property_name\n"
            "{\n"
            "\n"
            "    case $gl_property_type in\n"
            "        b)   echo $gl_old_property_value | grep -i true > /dev/null\n"
            "             if [ $? -eq 0 ]; then\n"
            "                gl_new_property_value=\"false\"\n"
            "             else\n"
            "                gl_new_property_value=\"true\"\n"
            "             fi\n"
            "             ;;\n"
            "        s)   gl_new_property_value=$( date +%F%X )\n"
            "             sleep 1\n"
            "             ;;\n"
            "        ## that should work for integers and double types\n"
            "        *)   change_value_method ${gl_property_value};;\n"
            "    esac\n"
            "\n"
            "    cmd=\"busctl set-property $SERVICE $@ $gl_property_type"
            " \'$gl_new_property_value\'\"\n"
            "    echo \"property_set(): $cmd\"\n"
            "    [ $DRY_RUN -eq 1 ] && return\n"
            "    eval $cmd 2> $gl_error_file\n"
            "    gl_rc=$?\n"
            "    if [ $gl_rc -eq 0 -a  $gl_debug -eq 0 ]; then\n"
            "       gl_injections=$[ $gl_injections + 1 ]\n"
            "    fi\n"
            "    return $gl_rc\n"
            "}\n"
            "\n"
            "matchDevice() # $1=device to match  $2=entryid to check\n"
            "{                     \n"
            "    device=$(busctl get-property xyz.openbmc_project.Logging /xyz/openbmc_project/logging/entry/$2 xyz.openbmc_project.Logging.Entry AdditionalData  \\\n"
            "              | awk -F 'namespace='   '{print $2}' |  awk '{print $0}' | tr -d \\\")\n"
            "    to_match=$(echo $1 | awk -F  / '{print $NF}')\n"
            "    if [ \"$to_match\" = \"$device\" ]; then\n"
            "       echo $2 ## correct entry id\n"
            "    fi\n"
            "}\n"
            "\n"
            "verifyInjection()\n"
            "{\n"
            "    seconds=10\n"
            "    local logid=\"\" # more then one event can be created\n"
            "    while [ \"$logid\" = \"\" -a $seconds -gt 0 ]; do\n"
            "       echo -ne waiting for the event, trying at most $seconds times to get a new EventLogId ...\r \n"
            "       sleep 1\n"
            "       local new_logging_entries=$(latest_looging_entry)\n"
            "       local expected_logging_entry=$[ $gl_current_logging_entries + 1 ]\n"
            "       if [ $new_logging_entries -gt $gl_current_logging_entries ]; then\n"
            "          while [ \"$logid\" = \"\" -a $expected_logging_entry -le $new_logging_entries ]; do\n"
            "               logid=$(matchDevice $1 $expected_logging_entry)\n"
            "               expected_logging_entry=$[$expected_logging_entry + 1]\n"
            "          done\n"
            "       fi\n"
            "       seconds=$[ $seconds - 1 ]\n"
            "    done\n"
            "    if [ \"$logid\" != \"\" ]; then\n"
            "       echo -ne \"\r\"\n"
            "       showStatus \"created EventLogId [ $logid ]\"\n"
            "       gl_injections=$[ $gl_injections + 1 ]\n"
            "    else\n"
            "       gl_rc=1\n"
            "       showStatus 'busctl commands worked but Event NOT created'\n"
            "    fi\n"
            "}\n"
            "\n"
            "showStatus()\n"
            "{\n"
            "    status='injected'\n"
            "    [ $gl_rc -ne 0 ] && status='failed'\n"
            "    error_msg=$(cat $gl_error_file 2> /dev/null)\n"
            "    message=\"[debug: $gl_counter_commands/$gl_total_comands] $status EVENT='$EVENT' : '$1' '$error_msg'\"\n"
            "    echo $message\n"
            "}\n"
            "\n"
            "property_change() #1=object_path $2=interface, $3=property_name\n"
            "{\n"
            "    gl_rc=0\n"
            "    gl_current_logging_entries=0\n"
            "    gl_counter_commands=$[ $gl_counter_commands + 1 ]\n"
            "    echo\n"
            "    if [ $gl_total_comands -ne 0 ]; then\n"
            "       echo -n \"injecting [$gl_counter_commands/$gl_total_comands]\"\n"
            "       echo \" EVENT='$EVENT' ACCESSOR_TYPE='$ACCESSOR_TYPE'\"\n"
            "    else\n"
            "       echo ============\n"
            "    fi\n"
            "\n"
            "    if [ \"$METHOD\" = \"skip\" ]; then\n"
            "       echo \"[skipping \'$METHOD_VALUE\'] $@\"\n"
            "       echo\n"
            "       return\n"
            "    fi\n"
            "    property_get \"$@\"  \\\n"
            "        &&  gl_current_logging_entries=$(latest_looging_entry) \\\n"
            "        &&  property_set_method \"$@\"  \\\n"
            "        &&  property_get \"$@\"\n"
            "\n"
            "   [ $DRY_RUN -eq 1 ] && return\n"
            "   if [ $gl_rc -eq 0 ]; then\n"
            "       [ $gl_debug -eq 1 ] && verifyInjection $1\n"
            "   else\n"
            "       showStatus 'busctl command failed'\n"
            "   fi\n"
            "}\n"
            "\n"
            "\n")


    def close_script_file(self):
        """
        override parent method, does nothing, close will be called from super()
        """


    def get_accessor_dbus_expected_events_list(self):
        """
        Returns a list of event data which has accessor.type = DBUS
        """
        return self._accessor_dbus_expected_events_list


    def __get_accessor_type(self):
        """
        Returns None or com.ACCESSOR_TYPE_DBUS
        """
        ret = "none"
        if com.KEY_ACCESSOR_TYPE in self._additional_data:
            accessor_type = self._additional_data[com.KEY_ACCESSOR_TYPE]
            ret = accessor_type
            if accessor_type.upper() == com.ACCESSOR_TYPE_DBUS:
                try:
                    if len(self._additional_data[com.KEY_ACCESSOR_OBJECT]) > 0 and \
                        len(self._additional_data[com.KEY_ACCESSOR_INTERFACE])  > 0 and \
                        len(self._additional_data[com.KEY_ACCESSOR_PROPERTY])   > 0:
                        ret = com.ACCESSOR_TYPE_DBUS
                except Exception as exc:
                    raise Exception(f"No information {com.KEY_ACCESSOR}") from exc
        return ret


    def __store_command_information(self, device, method, method_value, accessor_type):
        """
        Stores command information in the list self._commands_information_list
        This information is used to generate 'busctl 'commands later
        """
        information = {}
        information["device"] = device
        information["method"] = method
        information["method_value"] = method_value
        information["accessor_type"] = accessor_type
        information["event"]         = self._busctl_info[com.INDEX_EVENT]
        if com.KEY_ACCESSOR_INTERFACE in self._additional_data:
            information[com.KEY_ACCESSOR_INTERFACE] = \
                self._additional_data[com.KEY_ACCESSOR_INTERFACE]
            information[com.KEY_ACCESSOR_PROPERTY] = \
                self._additional_data[com.KEY_ACCESSOR_PROPERTY]
        self._commands_information_list.append(information)


    def __write_busctl_commands_into_script(self, cmd_info):
        """
        Saves busctl commands in the script
        """
        cmd =  f"\n METHOD=\"{cmd_info['method']}\" METHOD_VALUE=\"{cmd_info['method_value']}\" "
        cmd += f"ACCESSOR_TYPE=\"{cmd_info['accessor_type']}\" "
        if len(self._busctl_info) > com.INDEX_EVENT:
            cmd += f"EVENT=\"{cmd_info['event']}\" "
        cmd += f"property_change {cmd_info['device']} "
        if com.KEY_ACCESSOR_INTERFACE in cmd_info and com.KEY_ACCESSOR_PROPERTY in cmd_info:
            cmd += f"{cmd_info[com.KEY_ACCESSOR_INTERFACE]} "
            cmd += f"{cmd_info[com.KEY_ACCESSOR_PROPERTY]}\n"
        super().write(cmd)


    def __store_in_expected_events_list(self, device):
        """
        Stores event data to be later compared with RedFish data
        """
        event_data = self._additional_data.copy()
        super().remove_accessor_fields(event_data)
        event_data['event'] = self._busctl_info[com.INDEX_EVENT]
        event_data['device'] = device
        self._accessor_dbus_expected_events_list.append(event_data)


    def store_busctl_commands_as_dbus_accessor(self, device, accessor_type):
        """
        Generates commands in the script regardless the accessor data
        """
        method="add"
        value="1"
        if com.KEY_ACCESSOR_CHECK_BITMASK in self._additional_data:
            method="bitmask"
            value = self._additional_data[com.KEY_ACCESSOR_CHECK_BITMASK]
        elif com.KEY_ACCESSOR_CHECK_LOOKUP in self._additional_data:
            method="lookup"
            value=self._additional_data[com.KEY_ACCESSOR_CHECK_LOOKUP]
        if com.KEY_ACCESSOR_OBJECT in self._additional_data:
            device_range = com.expand_range(self._additional_data[com.KEY_ACCESSOR_OBJECT])
            for device_item in device_range:
                self.__store_command_information(device_item, method, value, accessor_type)
                self.__store_in_expected_events_list(device_item)
        else:
            self.__store_command_information(device, method, value, accessor_type)
            self.__store_in_expected_events_list(device)



    def generate_busctl_command_from_json_dict(self, device, data):
        """
        Redefines parent method
        Generates commands in the script if the accessor_type is not empty and is handled
        """
        super().parse_json_dict_data(device, data)
        accessor_type = self.__get_accessor_type()
        self.store_busctl_commands_as_dbus_accessor(device, accessor_type)

        ## Block commented, removed skipping, everything is performed
        #--------------------------------------------------------------
        #if  accessor_type == com.ACCESSOR_TYPE_DBUS:
            #if self._additional_data[com.KEY_ACCESSOR_OBJECT].upper() == "TBD" or \
                  #self._additional_data[com.KEY_ACCESSOR_INTERFACE].upper() == "TBD" or \
                  #self._additional_data[com.KEY_ACCESSOR_PROPERTY].upper() == "TBD":
                #method="skip"
                #value="any DBUS accessor field = TBD"
                #self.__store_command_information(device, method, value)
            #else:
                #self.store_busctl_commands_as_dbus_accessor(device, accessor_type)
        #else:
            #method ="skip"
            #value = f"accessor.type = {accessor_type}"
            #self.__store_command_information(device, method, value, accessor_type)
        #--------------------------------------------------------------
        super().remove_accessor_fields()


    def generate_script_from_json(self):
        """
        Redefines from super

        Calls generate_script_from_json() from super which does not generate
          the commands using its empy close_script_file()
        Writes the total of commands to be executed, it allows having a percentage
        Then writes all the commands
        """
        super().generate_script_from_json()
        total_commands = len(self._commands_information_list)
        if total_commands > 0:
            super().write(f"\ngl_total_comands={total_commands}\n\n")
            for cmd_info in self._commands_information_list:
                self.__write_busctl_commands_into_script(cmd_info)
            bottom = "\n\necho; echo \"Successful Injections: $gl_injections\"\n"
            super().priv_close_script_file(bottom)

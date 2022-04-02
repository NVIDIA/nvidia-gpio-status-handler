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
            "gl_injections=0\n"
            "gl_debug=0\n"
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
            "grep /xyz/openbmc_project/logging/entry/ | awk -F / '{ print $NF}')\n"
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
            "    local get_value=$(eval $cmd)\n"
            "    echo \" $get_value\"\n"
            "    if [ $? -ne 0 ]; then\n"
            "        gl_rc=1\n"
            "    else\n"
            "        gl_property_value=$(echo $get_value | awk '{print $NF}')\n"
            "        gl_property_type=$(echo $get_value | awk '{print $(NF-1)}')\n"
            "        [ \"$gl_property_value\" = \"\" -o \"$gl_property_type\" = \"\" ] && gl_rc=1\n"
            "    fi\n"
            "    return $gl_rc\n"
            "}\n"
            "\n"
            "\n"
            "property_set() #1=object_path $2=interface, $3=property_name\n"
            "{\n"
            "    gl_new_property_value=\"\"\n"
            "    gl_old_property_value=$gl_property_value\n"
            "\n"
            "    case $gl_property_type in\n"
            "        b)   if [ \"$gl_old_property_value\" = \"True\" ]; then\n"
            "                gl_new_property_value=\"False\"\n"
            "             else\n"
            "                gl_new_property_value=\"True\"\n"
            "             fi\n"
            "             ;;\n"
            "        s)   gl_new_property_value=$( date +%F%X )\n"
            "             sleep 1\n"
            "             ;;\n"
            "        ## that should work for integers and double types\n"
            "        *)   gl_new_property_value=$[ ${gl_property_value} + 1 ];;\n"
            "    esac\n"
            "\n"
            "    cmd=\"busctl set-property $SERVICE $@ $gl_property_type"
            " \'$gl_new_property_value\'\"\n"
            "    echo \"property_set(): $cmd\"\n"
             "   [ $DRY_RUN -eq 1 ] && return\n"
            "    eval $cmd\n"
            "    gl_rc=$?\n"
            "    if [ $gl_rc -eq 0 ]; then\n"
            "       gl_injections=$[ $gl_injections + 1 ]\n"
            "    fi\n"
            "    return $gl_rc\n"
            "}\n"
            "\n"
            "verifyInjection()\n"
            "{\n"
            "   new_logging_entries=$(latest_looging_entry)\n"
            "    local expected_logging_entry=$[ $current_logging_entries + 1 ]\n"
            "    if [ $new_logging_entries -gt $current_logging_entries ]; then\n"
            "       echo [debug] created EventLog Id $new_logging_entries\n"
            "    else\n"
            "       echo [debug] EventLog NOT created\n"
            "    fi\n"
            "}\n"
            "\n"
            "property_change() #1=object_path $2=interface, $3=property_name\n"
            "{\n"
            "    gl_rc=0\n"
            "    local current_logging_entries=0\n"
            "    local new_logging_entries=0\n"
            "    echo\n"
            "    echo ============\n"
            "\n"
            "    property_get \"$@\"  \\\n"
            "        &&  current_logging_entries=$(latest_looging_entry) \\\n"
            "        &&  property_set \"$@\"  \\\n"
            "        &&  property_get \"$@\"\n"
            "\n"
            "   if [ $gl_rc -eq 0 ]; then\n"
            "       [ $gl_debug -eq 1 ] && verifyInjection\n"
            "   else\n"
            "       echo [debug] busctl failed\n"
            "   fi\n"
            "}\n"
            "\n"
            "\n")


    def close_script_file(self):
        """
        override parent method
        """
        super().priv_close_script_file("\n\necho; echo \"Successful Injections: $gl_injections\"\n")


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
            if accessor_type.upper() == com.ACCESSOR_TYPE_DBUS:
                try:
                    if len(self._additional_data[com.KEY_ACCESSOR_OBJECT]) > 0 and \
                        len(self._additional_data[com.KEY_ACCESSOR_INTERFACE])  > 0 and \
                        len(self._additional_data[com.KEY_ACCESSOR_PROPERTY])   > 0:
                        ret = com.ACCESSOR_TYPE_DBUS
                except Exception as exc:
                    raise Exception(f"No information {com.KEY_ACCESSOR}") from exc
        return ret


    def __generate_busctl_command_from_json_accessor_dbus(self, device):
        """
        Saves bustcl commands in the script
        """
        cmd =  f"\nproperty_change {device} "
        cmd += f"{self._additional_data[com.KEY_ACCESSOR_INTERFACE]} "
        cmd += f"{self._additional_data[com.KEY_ACCESSOR_PROPERTY]}\n"
        super().write(cmd)


    def generate_busctl_command_from_json_dict(self, device, data):
        """
        Redefines parent method
        Generates commands in the script if the accessor_type is not empty and is handled
        """
        counter = 0
        super().parse_json_dict_data(device, data)
        if self.__get_accessor_type() == com.ACCESSOR_TYPE_DBUS:
            for device_item in com.expand_range(self._additional_data[com.KEY_ACCESSOR_OBJECT]):
                self.__generate_busctl_command_from_json_accessor_dbus(device_item)
                counter += 1
            super().remove_accessor_fields()
            while counter > 0:
                self._accessor_dbus_expected_events_list.append(self._additional_data.copy())
                counter -= 1


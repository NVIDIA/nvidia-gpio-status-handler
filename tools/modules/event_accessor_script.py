import json
import tempfile
import os


from injector_script_base import InjectorScriptBase


class EventAccessorInjectorScript(InjectorScriptBase):
    """
    This class parses a Json Event file, then:
       1 Generates a bash script with busctl commands
    """

    def __init__(self, json_file):
        super().__init__(json_file)
        self._busctl_cmd_counter = 0
        self._KEY_ACCESSOR = "Accessor"
        self._KEY_ACCESSOR_TYPE      = self._KEY_ACCESSOR + ".Type"
        self._KEY_ACCESSOR_OBJECT    = self._KEY_ACCESSOR + ".Object"
        self._KEY_ACCESSOR_INTERFACE = self._KEY_ACCESSOR + ".Interface"
        self._KEY_ACCESSOR_PROPERTY  = self._KEY_ACCESSOR + ".Property"
        self._ACCESSOR_TYPE_DBUS     = "DBUS"


    def create_script_file(self):
        """
        override parent method
        """
        super().priv_create_script_file("#!/bin/bash\n"
                                    "\n"
                                    "SERVICE=xyz.openbmc_project.GpuMgr\n"
                                    "\n"
                                    "## global variables\n"
                                    "gl_property_value=\"\"\n"
                                    "gl_property_type=\"\"\n"
                                    "gl_new_property_value=\"\"\n"
                                    "gl_old_property_value=\"\"\n"
                                    "gl_rc=0\n"
                                    "gl_script_rc=0\n"
                                    "\n"
                                    "\n"
                                    "latest_looging_entry()\n"
                                    "{\n"
                                    "   latest_entry=0\n"
                                    "   for entry in  $(busctl tree xyz.openbmc_project.Logging | grep /xyz/openbmc_project/logging/entry/ | awk -F / '{ print $NF}')\n"
                                    "   do\n"
                                    "      [ $entry -gt $latest_entry ] && latest_entry=$entry\n"
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
                                    "    echo -n \"property_get(): $cmd\"\n"
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
                                    "        s)   gl_new_property_value=\"${gl_property_value}_${COUNTER}\";;\n"
                                    "        d)   gl_new_property_value=$[ ${gl_property_value} + 1 ];;\n"
                                    "    esac\n"
                                    "\n"
                                    "    cmd=\"busctl set-property $SERVICE $@ $gl_property_type \'$gl_new_property_value\'\"\n"
                                    "    echo \"property_set(): $cmd\"\n"
                                    "    eval $cmd\n"
                                    "    gl_rc=$?\n"
                                    "    return $gl_rc\n"
                                    "}\n"
                                    "\n"
                                    "\n"
                                    "property_change() #1=object_path $2=interface, $3=property_name\n"
                                    "{\n"
                                    "    gl_rc=0\n"
                                    "    local current_logging_entries=0\n"
                                    "    local new_logging_entries=0\n"
                                    "    echo\n"
                                    "\n"
                                    "    property_get \"$@\"  \\\n"
                                    "        &&  current_logging_entries=$(latest_looging_entry) \\\n"
                                    "        &&  property_set \"$@\"  \\\n"
                                    "        &&  property_get \"$@\"\n"
                                    "\n"
                                    "    if [ $gl_rc -ne 0 -o \"$gl_new_property_value\" != \"$gl_property_value\" ]; then\n"
                                    "        echo \"property_change(): Failed\"\n"
                                    "        gl_script_rc=1\n"
                                    "    else\n"
                                    "        new_logging_entries=$(latest_looging_entry)\n"
                                    "        local expected_logging_entry=$[ $current_logging_entries + 1 ]\n"
                                    "        if [ $new_logging_entries -gt $current_logging_entries ]; then\n"
                                    "            echo \"property_change(): Passed, value changed from '$gl_old_property_value' to '$gl_property_value', EventLog Id = $new_logging_entries\"\n"
                                    "        else\n"
                                    "            echo \"property_change(): Failed, value changed from '$gl_old_property_value' to '$gl_property_value', But Logging was not created, expected EventLog Id $expected_logging_entry\"\n"
                                    "            gl_script_rc=1\n"
                                    "        fi\n"
                                    "    fi\n"
                                    "}\n"
                                    "\n"
                                    "\n")



    def close_script_file(self):
        """
        override parent method
        """
        super().priv_close_script_file("\n\nexit $gl_script_rc\n")


    def __format_additional_data_copule(self, ad_key):
        return ' \\\n\t'  + ad_key + ' \\"' + self._additional_data[ad_key] + '\\"'


    def __create_bustcl_command(self):
        cmd  = '''busctl call xyz.openbmc_project.Logging /xyz/openbmc_project/logging '''
        cmd += "xyz.openbmc_project.Logging.Create Create ssa{ss} "
        cmd += '\\"' + self._busctl_info[self._INDEX_DEVICE_NAME] + '\\" '
        cmd +=  self._busctl_info[self._INDEX_SEVERITY] +  ' ' + str(len(self._additional_data))
        try:
            cmd += self.__format_additional_data_copule(self._key_REDFISH_MESSAGE_ID)
            cmd += self.__format_additional_data_copule(self._key_REDFISH_MESSAGE_ARGS)
            del self._additional_data[self._key_REDFISH_MESSAGE_ID]
            del self._additional_data[self._key_REDFISH_MESSAGE_ARGS]
        except Exception as e:
            raise Exception(f"Json either 'REDFISH_MESSAGE_ID' or 'REDFISH_MESSAGE_ARGS' information missing: {str(e)}")
        for ad_key in sorted(self._additional_data.keys()):
            cmd += self.__format_additional_data_copule(ad_key)
        self._busctl_cmd_counter += 1
      #  print("\ncounter=%03d command=%s" % (self._busctl_cmd_counter, cmd))
        return cmd


    def __parse_dict_data(self, device_name, data_device):
        self._additional_data.clear()
        fields_device = list(data_device.keys())
        ret = "none"
        for field in fields_device:
            if field.lower() == self._KEY_ACCESSOR.lower():
                value = data_device[field]
                super().parse_json_sub_dict_field(field, value, None)
        if self._KEY_ACCESSOR_TYPE in self._additional_data:
            type = self._additional_data[self._KEY_ACCESSOR_TYPE]
            if type.upper() == self._ACCESSOR_TYPE_DBUS:
                try:
                   if len(self._additional_data[self._KEY_ACCESSOR_OBJECT]) > 0 and \
                       len(self._additional_data[self._KEY_ACCESSOR_INTERFACE])  > 0 and \
                       len(self._additional_data[self._KEY_ACCESSOR_PROPERTY])   > 0:
                           ret = self._ACCESSOR_TYPE_DBUS
                except Exception as e:
                    raise Exception(f"Mising informartion for {self._KEY_ACCESSOR} in device {device_name}")
        return ret


    def __generate_busctl_command_from_json_accessor_dbus(self, device):
        cmd =  f"\nproperty_change {device} "
        cmd += f"{self._additional_data[self._KEY_ACCESSOR_INTERFACE]} "
        cmd += f"{self._additional_data[self._KEY_ACCESSOR_PROPERTY]}\n"
        super().write(cmd)


    def generate_busctl_command_from_json_dict(self, device, data):
        if self.__parse_dict_data(device, data) == self._ACCESSOR_TYPE_DBUS:
            print(f"device {device} data {self._additional_data}")
            for device_item in super().expand_range(self._additional_data[self._KEY_ACCESSOR_OBJECT]):
                self.__generate_busctl_command_from_json_accessor_dbus(device_item)
        else:
            print(f"device {device} Accessor not DBUS")






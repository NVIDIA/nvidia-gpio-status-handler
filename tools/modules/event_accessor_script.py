"""
Module for handling devices from Json input file which as 'accessor' information not empty
So far accessor.type = DBUS are handled
"""

import com ## common constants and and functions

from injector_script_base import InjectorScriptBase

TEMPLATE_NAME = "event_accessor_script.bash"

class EventAccessorInjectorScript(InjectorScriptBase):
    """
    This class parses a Json Event file, then:
       1 Generates a bash script with busctl commands
    """

    def __init__(self, json_file):
        super().__init__(json_file)
        self._busctl_cmd_counter = 0
        self._accessor_dbus_expected_events_list = []
        self._max_commands = 0 # if greater than zero tells the maximum commands
        """
        Keeps commands in a list just to have the total counter
        Then comands are generated later
        """
        self._commands_information_list = []


    def create_script_file(self):
        """
        override parent method
        """
        template_file = com.TEMPLATES_DIR + '/' + TEMPLATE_NAME
        template = open(template_file, mode='r')
        content=""
        ok = False
        if template != None:
            content = template.read()
            template.close()
            if len(content) != 0:
               ok = True
        if ok is False:
               raise Exception(f"template file '{template_file}' not found or empty")

        super().priv_create_script_file(content)

    def close_script_file(self):
        """
        override parent method, does nothing, close will be called from super()
        """


    def get_accessor_dbus_expected_events_list(self):
        """
        Returns a list of event data which has accessor.type = DBUS
        """
        return self._accessor_dbus_expected_events_list


    def __join_event_trigger_and_accessor(self):
        """
        move event_trigger information into accessor
        set accessor.dependency.type       from accessor.type
            accessor.dependency.property   from accessor.property
        """
        accessor_key = f"{com.KEY_ACCESSOR}."
        for key in list(self._additional_data.keys()):
            if key.startswith(accessor_key):
                dep_field = key.split(accessor_key)[1]
                dep_key = com.KEY_ACCESSOR_DEP + '.' + dep_field
                self._additional_data[dep_key] = self._additional_data[key]
                del self._additional_data[key]

        event_trigger_key = f"{com.KEY_EVENT_TRIGGER}."
        for key in list(self._additional_data.keys()):
            if key.startswith(event_trigger_key):
                accessor_field = key.split(event_trigger_key)[1]
                accessor_key   = com.KEY_ACCESSOR + '.' + accessor_field;
                self._additional_data[accessor_key] = self._additional_data[key]
                del self._additional_data[key]


    def __get_accessor_type(self):
        """
        Returns None or com.ACCESSOR_TYPE_DBUS or ACCESSOR_TYPE_DBUS_CALL
        """
        ret = "none"
        if com.KEY_EVENT_TRIGGER_TYPE in self._additional_data:
            self.__join_event_trigger_and_accessor()
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
                    raise Exception(f"No information {com.ACCESSOR_TYPE_DBUS}") from exc
            elif accessor_type.upper() == com.ACCESSOR_TYPE_DBUS_CALL:
                if com.KEY_ACCESSOR_PROPERTY in self._additional_data and \
                    len(self._additional_data[com.KEY_ACCESSOR_PROPERTY]) > 0:
                        ret = com.ACCESSOR_TYPE_DBUS_CALL
                else:
                    # so far consider this as DUBS to generate wrong set-property command
                    ret = com.ACCESSOR_TYPE_DBUS
        return ret


    def __store_command_information(self, information, device_index):
        """
        Stores command information in the list self._commands_information_list
        This information is used to generate 'busctl 'commands later
        """
        if device_index != -1 and len(com.SINGLE_DEVICE_INDEX) > 0:
           if str(device_index) != com.SINGLE_DEVICE_INDEX:
              return False

        information["range"]  = com.DEVICE_RANGE
        information["device"] = com.DEVICE_HW
        information["event"]         = self._busctl_info[com.INDEX_EVENT]
        if device_index != -1:
            information["device_index"] = device_index
        else:
            information["device_index"] = 0
        if com.KEY_ACCESSOR_INTERFACE in self._additional_data:
            information[com.KEY_ACCESSOR_INTERFACE] = \
                self._additional_data[com.KEY_ACCESSOR_INTERFACE]
        if com.KEY_ACCESSOR_PROPERTY in self._additional_data:
            information[com.KEY_ACCESSOR_PROPERTY] = \
                self._additional_data[com.KEY_ACCESSOR_PROPERTY]
        self._commands_information_list.append(information.copy())

        return True


    def __write_busctl_commands_into_script(self, cmd_info):
        """
        Saves busctl commands in the script
        """
        cmd =  f"\n METHOD=\"{cmd_info['method']}\" METHOD_VALUE=\"{cmd_info['method_value']}\" "
        cmd += f"ACCESSOR_TYPE=\"{cmd_info['accessor_type']}\" "
        cmd += f" \\\n\tINJECTION=\"{cmd_info['injection']}\" INJECTION_PARAM=\"{cmd_info['injection_param']}\" "
        cmd += f"INJECTION_CHECK=\"{cmd_info['injection_check']}\" INJECTION_CHECK_VALUE=\"{cmd_info['injection_check_value']}\""
        if len(self._busctl_info) > com.INDEX_EVENT:
            cmd += f" \\\n\tDEVICE=\"{cmd_info['device']}\" DEVICE_RANGE=\"{cmd_info['range']}\" DEVICE_INDEX=\"{cmd_info['device_index']}\" EVENT=\"{cmd_info['event']}\" "

        cmd += f" \\\n\tproperty_change {cmd_info['device_item']} "
        if com.KEY_ACCESSOR_INTERFACE in cmd_info and com.KEY_ACCESSOR_PROPERTY in cmd_info:
            cmd += f"{cmd_info[com.KEY_ACCESSOR_INTERFACE]} "
            cmd += f"{cmd_info[com.KEY_ACCESSOR_PROPERTY]}\n"
        else:
            cmd += "\n"
        super().write(cmd)


    def __store_in_expected_events_list(self, device):
        """
        Stores event data to be later compared with RedFish data
        """
        event_data = self._additional_data.copy()
        super().remove_accessor_fields(event_data)
        event_data['event'] = self._busctl_info[com.INDEX_EVENT]
        event_data['device'] = com.DEVICE_HW
        self._accessor_dbus_expected_events_list.append(event_data)


    def set_injection_information(self, information):
        """
        """
        information['injection'] = "DBUS"
        information['injection_param'] = ""
        information['injection_check'] = ""
        information['injection_check_value'] = ""

        if com.KEY_ACCESSOR_DEP_TYPE in self._additional_data and \
           self._additional_data[com.KEY_ACCESSOR_DEP_TYPE] == com.ACCESSOR_TYPE_DEVICE_CORE_API:
            information['injection'] =  com.ACCESSOR_TYPE_DEVICE_CORE_API
            information['injection_param'] = self._additional_data[com.KEY_ACCESSOR_DEP_PROPERTY]
            if com.KEY_ACCESSOR_DEP_CHECK_LOOKUP in self._additional_data:
               information['injection_check'] = "lookup"
               information['injection_check_value'] = self._additional_data[com.KEY_ACCESSOR_DEP_CHECK_LOOKUP]
            elif com.KEY_ACCESSOR_DEP_CHECK_EQUAL  in self._additional_data:
               information['injection_check'] = "equal"
               information['injection_check_value'] = self._additional_data[com.KEY_ACCESSOR_DEP_CHECK_EQUAL]
            return

        if com.KEY_ACCESSOR_TYPE in self._additional_data and \
           self._additional_data[com.KEY_ACCESSOR_TYPE] == com.ACCESSOR_TYPE_DEVICE_CORE_API:
            information['injection'] =  com.ACCESSOR_TYPE_DEVICE_CORE_API
            information['injection_param'] = self._additional_data[com.KEY_ACCESSOR_PROPERTY]
            return

        if com.KEY_ACCESSOR_DEP_TYPE in self._additional_data and \
           self._additional_data[com.KEY_ACCESSOR_DEP_TYPE] == com.ACCESSOR_TYPE_CMDLINE:
            information['injection'] =  com.ACCESSOR_TYPE_CMDLINE
            information['injection_param'] = self._additional_data[com.KEY_ACCESSOR_DEP_EXECUTABLE] + \
                                             " " +  self._additional_data[com.KEY_ACCESSOR_DEP_ARGUMENTS]
            return

        if com.KEY_ACCESSOR_TYPE in self._additional_data and \
           self._additional_data[com.KEY_ACCESSOR_TYPE] == com.ACCESSOR_TYPE_CMDLINE:
            information['injection'] =  com.ACCESSOR_TYPE_CMDLINE
            information['injection_param'] = self._additional_data[com.KEY_ACCESSOR_EXECUTABLE] + \
                                             " " +  self._additional_data[com.KEY_ACCESSOR_ARGUMENTS]
            return


    def store_busctl_commands_as_dbus_accessor(self, device, accessor_type):
        """
        Generates commands in the script regardless the accessor data
        """
        method="add"
        value="1"

        if com.KEY_ACCESSOR_CHECK_EQUAL in self._additional_data:
            method="equal"
            value = self._additional_data[com.KEY_ACCESSOR_CHECK_EQUAL]
        elif com.KEY_ACCESSOR_CHECK_NOTEQUAL in self._additional_data:
            method="not_equal"
            value = self._additional_data[com.KEY_ACCESSOR_CHECK_NOTEQUAL]
        elif com.KEY_ACCESSOR_CHECK_BITMASK in self._additional_data:
            method="bitmask"
            value = self._additional_data[com.KEY_ACCESSOR_CHECK_BITMASK]
            #------ do change bitmask
            #if com.KEY_ACCESSOR_DEP_TYPE not in self._additional_data:
                #method="bitmask"
                #value = self._additional_data[com.KEY_ACCESSOR_CHECK_BITMASK]
            #elif com.KEY_ACCESSOR_DEP_PROPERTY in self._additional_data:
                #method = "bitmask_dbus_call"
                #value =  f"{com.CURRENT_JSON_DEVICE_INDEX}:{self._additional_data[com.KEY_ACCESSOR_DEP_PROPERTY]}"
            #-----------
        elif com.KEY_ACCESSOR_CHECK_LOOKUP in self._additional_data:
            method="lookup"
            value=self._additional_data[com.KEY_ACCESSOR_CHECK_LOOKUP]

        information = {}
        self.set_injection_information(information)

        information["device_item"] = device
        information["method"] = method
        information["method_value"] = value
        information["accessor_type"] = accessor_type
        com.DEVICE_RANGE = ""
        device_index = 0
         ## starts indicating there is no range
        com.INITIAL_DEVICE_RANGE = -1
        if com.KEY_ACCESSOR_OBJECT in self._additional_data:
            range_str = com.get_range(self._additional_data[com.KEY_ACCESSOR_OBJECT])
            if len(range_str) > 0:
               com.DEVICE_RANGE = range_str
               values_range = range_str.split('-')
               com.INITIAL_DEVICE_RANGE = int(values_range[0][1:])
               device_index = com.INITIAL_DEVICE_RANGE
               com.FINAL_DEVICE_RANGE   = int(values_range[0][1:])
            device_range = com.expand_range(self._additional_data[com.KEY_ACCESSOR_OBJECT])
            for device_item in device_range:
                information["device_item"] = device_item
                if self.__store_command_information(information, device_index) is True:
                   self.__store_in_expected_events_list(device_item)
                device_index = device_index + 1
        else:
            if self.__store_command_information(information, device_index) is True:
               self.__store_in_expected_events_list(device)



    def generate_busctl_command_from_json_dict(self, device, data):
        """
        Redefines parent method
        Generates commands in the script if the accessor_type is not empty and is handled
        """
        super().parse_json_dict_data(device, data)
        should_generate_commands = True
        event = data["event"]

        if com.LIST_EVENTS is True:
            range_str = com.get_range(data['device_type'])
            print (f"{com.DEVICE_HW}{range_str} \"{event}\"")
            should_generate_commands = False
        else:
            if len(com.SINGLE_DEVICE) > 0  and com.DEVICE_HW != com.SINGLE_DEVICE:
               should_generate_commands = False
            elif len(com.SINGLE_EVENT) > 0  and event != com.SINGLE_EVENT:
               should_generate_commands = False

        if should_generate_commands is True:
           accessor_type = self.__get_accessor_type()
           self.store_busctl_commands_as_dbus_accessor(device, accessor_type)
        else:
           event_data = self._additional_data.copy()
           super().remove_accessor_fields(event_data)

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
            if self._max_commands > 0 and  total_commands > self._max_commands:
                total_commands = self._max_commands;
            super().write(f"\ngl_total_comands={total_commands}\n\n")
            for cmd_info in self._commands_information_list:
                self.__write_busctl_commands_into_script(cmd_info)
                total_commands -= 1
                if total_commands == 0:
                    break

            bottom ="\n\nfinish_test ## release/clear everything\n\nexit $gl_rc"
            super().priv_close_script_file(bottom)

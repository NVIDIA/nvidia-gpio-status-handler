"""
Base module to handle bash scripts creation
"""

import json
import tempfile
import os

import abc

import com ## common constants and and functions


class InjectorScriptBase(abc.ABC):
    """
    Abstract class to provide basic bash scripts creation facilities
    """

    def __init__(self, json_file):
        """
        Constructor
        """
        self._json_file  = json_file
        self._shell_file = ""
        self._shell_file_fd = 0
        self._additional_data = {}
        self._busctl_info             = ["", ""]  # 2 items only, 0=device name, 1=event


    def __del__(self):
        """
        Destructor
        """
        if len(self._shell_file) > 0 and os.access(self._shell_file, os.W_OK):
            os.unlink(self._shell_file)


    def __load_json_file(self):
        """
        Opens the Json file and returns its json struct
        """
        json_file_fd = 0
        data = 0
        try:
            json_file_fd = open(self._json_file)
        except Exception as e:
            raise Exception(f"Json events input file not found:  {self._json_file} : {str(e)}")
        try:
            data = json.load(json_file_fd)
            json_file_fd.close()
        except Exception as e:
            raise Exception(f"Json file not in a good format:  {self._json_file} : {str(e)}")
        return data


    @abc.abstractmethod
    def create_script_file(self):
        """
        Abstract method, this version just creates the generated script
        """
        self.priv_create_script_file(None)


    @abc.abstractmethod
    def close_script_file(self):
        """
        Abstract method, this version just closes the generated script
        """
        self.priv_close_script_file(None)


    @abc.abstractmethod
    def generate_busctl_command_from_json_dict(self, device, data):
        """
        Abstract pure method
        """
        pass


    def priv_create_script_file(self, header):
        """
        Creates the temporary script to be generated
        the header data is optional, writes it if not None
        """
        try:
            fd_temp, self._shell_file = tempfile.mkstemp()
            os.close(fd_temp)
            self._shell_file_fd = open(self._shell_file, 'w', encoding="utf-8")
            if len(header) > 0:
                self.write(header)
        except Exception as error:
            raise Exception(f"Could not create temporary file:  {str(e)}")


    def priv_close_script_file(self, footer):
        """
        Close the generated script, writes the footer data if it is not None
        """
        if len(footer) > 0:
            self.write(footer)
        self._shell_file_fd.close()


    def script_file(self):
        """
        Returns the pathname of the temporary script
        """
        return self._shell_file


    def write(self, data):
        """
        Writes data into the generated script
        """
        try:
            self._shell_file_fd.write(data)
        except Exception as e:
            raise Exception(f"Could not write data into file: {self._json_file} : {str(e)}")


    def __parse_json_mandatory_fields(self, field, value):
        """
        Stores mandatory values into self._busctl_info and self._additional_data
        """
        if field == "event":
            self._busctl_info[com.INDEX_DEVICE_NAME] +=  " " + value
            self._busctl_info[com.INDEX_EVENT]= value
        elif field == "redfish":
            self._additional_data[com.REDFISH_MESSAGE_ID] = value["message_id"]
        elif field == "severity":
            self._additional_data[com.KEY_SEVERITY] = com.get_logging_entry_level(value)


    @abc.abstractmethod
    def parse_json_dict_data(self, device_name, data_device):
        """
        Parses one Json element, values are stored into self._busctl_info and self._additional_data
        """
        self._busctl_info[com.INDEX_DEVICE_NAME] = device_name
        keys = data_device.keys()
        fields_device = list(keys)
        ## this is the position to put the counter of custom field_and_value
        for field in fields_device:
            value = data_device[field]
            if field in com.JSON_MANDATORY_FIELDS:
                self.__parse_json_mandatory_fields(field, value)
            elif field in com.JSON_ADDITIONAL_FIELDS:
                self.parse_json_sub_dict_field(field, value)


    def parse_json_sub_dict_field(self, field_str, value):
        """
        parse dict fiels recursively, keys as 'key_field'  are expanded to Parent.Child
        """
        first_char = field_str[0]
        field_str = first_char.upper() + field_str[1:]
        if isinstance(value, dict):
            subkeys = value.keys()
            for subfield in subkeys:
                subfield_str = subfield[0]
                subfield_str = subfield_str[0].upper() + subfield[1:]
                child_str = field_str + '.' + subfield_str
                self.parse_json_sub_dict_field(child_str, value[subfield])
        else:
            if isinstance(value, list):
                value = ", ".join(value)
            if not isinstance(value, str):
                value = str(value)
            self._additional_data[field_str] = value


    def generate_script_from_json(self):
        """
        1. parses the Json Events file
        2. generates the shell file with busctl commands
        """
        data = self.__load_json_file()
        key_list = list(data.keys())
        if len(key_list) > 0:
            self.create_script_file()
        for device in key_list:
            data_dict = data[device]
            self.generate_device_busctl_commands(device, data_dict)
        if len(key_list) > 0:
            self.close_script_file()


    def remove_accessor_fields(self):
        """
        Just remove these fields to use the entire content of self._additional_data
        """
        accessor_key = f"{com.KEY_ACCESSOR}."
        for key in list(self._additional_data.keys()):
            if key.startswith(accessor_key):
                    del self._additional_data[key]
        if com.KEY_ACCESSOR_TYPE in self._additional_data:
            del self._additional_data[com.KEY_ACCESSOR_TYPE]


    def generate_device_busctl_commands(self, device, data):
        """
        Just a conveniente function to be reused,
           calls the abstrac method generate_busctl_command_from_json_dict()
        """
        if isinstance(data, dict):
            self.generate_busctl_command_from_json_dict(device, data)
        else:
            for index in  range(len(data)):
                device_name = device+str(index)
                self.generate_busctl_command_from_json_dict(device_name, data[index])

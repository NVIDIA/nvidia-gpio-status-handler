import json
import tempfile
import os

import abc

class InjectorScriptBase(abc.ABC):

    def __init__(self, json_file):
        self._json_file  = json_file
        self._shell_file = ""
        self._shell_file_fd = 0
        self._additional_data = dict()


    def __del__(self):
         if len(self._shell_file) > 0 and os.access(self._shell_file, os.W_OK):
            os.unlink(self._shell_file)


    def __load_json_file(self):
        fd = 0
        data = 0
        try:
            fd = open(self._json_file)
        except Exception as e:
            raise Exception(f"Json events input file not found:  {self._json_file} : {str(e)}")
        try:
            data = json.load(fd)
            fd.close()
        except Exception as e:
            raise Exception(f"Json file not in a good format:  {self._json_file} : {str(e)}")
        return data


    @abc.abstractmethod
    def create_script_file(self):
        self.__priv_create_script_file(None)


    @abc.abstractmethod
    def close_script_file(self):
        self.__priv_close_script_file(None)


    @abc.abstractmethod
    def generate_busctl_command_from_json_dict(self, device, data):
        pass


    @abc.abstractmethod
    def generate_busctl_command_from_json_dict(self, device, data):
        pass


    def priv_create_script_file(self, header):
        try:
            fd, self._shell_file = tempfile.mkstemp()
            os.close(fd)
            self._shell_file_fd = open(self._shell_file, 'w')
            if len(header) > 0:
                self.write(header)
        except Exception as e:
            raise Exception(f"Could not create temporary file:  {str(e)}")


    def priv_close_script_file(self, bottom):
        if len(bottom) > 0:
                self.write(bottom)
        self._shell_file_fd.close()


    def script_file(self):
        return self._shell_file


    def write(self, data):
        try:
            self._shell_file_fd.write(data)
        except Exception as e:
            raise Exception(f"Could not write data into file: {self._json_file} : {str(e)}")


    def parse_json_sub_dict_field(self, field_str, value, key_prefix):
        first_char = field_str[0]
        field_str = first_char.upper() + field_str[1:]
        if isinstance(value, dict):
            subkeys = value.keys()
            for subfield in subkeys:
               subfield_str = subfield[0]
               subfield_str = subfield_str[0].upper() + subfield[1:]
               child_str = field_str + '.' + subfield_str
               self.parse_json_sub_dict_field(child_str, value[subfield], key_prefix)
        else:
            key_field = field_str
            if key_prefix is not None:
                key_field = key_prefix + field_str
            if isinstance(value, list):
                value = ", ".join(value)
            if not isinstance(value, str):
                value = str(value)
            self._additional_data[key_field] = value


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

    def generate_device_busctl_commands(self, device, data):
        if isinstance(data, dict):
            self.generate_busctl_command_from_json_dict(device, data)
        else:
            for index in  range(len(data)):
                device_name = device+str(index)
                self.generate_busctl_command_from_json_dict(device_name, data[index])


    '''
    expand_range(range_string) returns a list from strings with/without range specification at end

    expand a name such as:

        expand_range("name[1-5]")         -> ['name1', 'name2', 'name3', 'name4', 'name5']
        expand_range("unique")            -> ['unique']
        expand_range("other[0, 1 , 3-5]") -> ['other0', 'other1', 'other3', 'other4', 'other5']
        expand_range("chars[0, 1 , g-m]") -> ['chars0', 'chars1', 'charsg', 'charsh', 'charsi', 'charsj', 'charsk', 'charsl', 'charsm']

    '''
    def expand_range(self, name):
        name_prefix = name
        name_range = name.split('[')
        list_names = []
        if len(name_range) > 1:
            name_prefix = name_range[0];
            for item_string in name_range[1].split(','):
                item = item_string.strip()
                if '-' not in item:
                    list_names.append(name_prefix + item)
                else:
                    values_range = item.split('-')
                    value = values_range[0].strip()
                    final_value = values_range[1].strip()
                    while (value <= final_value):
                        list_names.append(name_prefix + value)
                        value_int = ord(value) + 1
                        value = chr(value_int)
        else:
            list_names.append(name_prefix)
        return list_names


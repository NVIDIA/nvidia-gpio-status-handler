"""
Common constants and functions
"""

import re

EVENT_LOG_URI="redfish/v1/Systems/system/LogServices/EventLog/Entries"

LOGGING_SERVICE = "xyz.openbmc_project.Logging"
LOGGING_OBJECT  = "/xyz/openbmc_project/logging"

# Constants used for Event Logging inserting in bustcl commands
REDFISH_MESSAGE_ID    = "REDFISH_MESSAGE_ID"
REDFISH_MESSAGE_ARGS  = "REDFISH_MESSAGE_ARGS"


# Not all fields from Json files are handled, two lists below:
JSON_MANDATORY_FIELDS  = ["redfish", "severity", "event"]
JSON_ADDITIONAL_FIELDS = ["resolution", "accessor"]


# Keys used in dicts
KEY_SEVERITY = 'Severity'
KEY_ACCESSOR = "Accessor"
KEY_ACCESSOR_TYPE      = KEY_ACCESSOR + ".Type"
KEY_ACCESSOR_OBJECT    = KEY_ACCESSOR + ".Object"
KEY_ACCESSOR_INTERFACE = KEY_ACCESSOR + ".Interface"
KEY_ACCESSOR_PROPERTY  = KEY_ACCESSOR + ".Property"


# common constants
ACCESSOR_TYPE_DBUS     = "DBUS"
LOGGING_ENTRY_STR     = "xyz.openbmc_project.Logging.Entry"
LOGGING_ENTRY_DOT_STR = f"{LOGGING_ENTRY_STR}."


# index in busctl_info
INDEX_DEVICE_NAME     = 0
INDEX_EVENT           = 1


# BUSCTL command index for Event Logging inserting -> Corresponding field position
BUSCTL_MSG_IDX            = 7   # "GPU0 OverT"
BUSCTL_SEVERITY_IDX       = 8   # "xyz.openbmc_project.Logging.Entry.Level.Critical"
BUSCTL_ADDITIONALDATA_IDX = 10  # where additinal data starts


# Keys used for comparing 'injected events' and 'redfish data'
#   if an element is a list: the first element is the key and the second is an alias
MANDATORY_EVENT_KEYS = [ KEY_SEVERITY, 'Resolution', ['MessageId', REDFISH_MESSAGE_ID] ]
OPTIONAL_EVENT_KEYS  = [ ['MessageArgs', REDFISH_MESSAGE_ARGS] ]

# Flags used for comparing 'injected events' and 'redfish data'
MANDATORY_FLAG       = True
OPTIONAL_FLAG        = False

SEVERITYDBUSTOREDFISH = {
                            'critical'  : [
                                        "xyz.openbmc_project.Logging.Entry.Level.Alert",
                                        "xyz.openbmc_project.Logging.Entry.Level.Critical",
                                        "xyz.openbmc_project.Logging.Entry.Level.Emergency",
                                        "xyz.openbmc_project.Logging.Entry.Level.Error"],
                            'ok'        : [
                                            "xyz.openbmc_project.Logging.Entry.Level.Debug",
                                            "xyz.openbmc_project.Logging.Entry.Level.Informational",
                                            "xyz.openbmc_project.Logging.Entry.Level.Notice"],
                            'warning'   : ["xyz.openbmc_project.Logging.Entry.Level.Warning"]
                        }


def get_logging_entry_level(level):
    """
    Just formats a entry level
    """
    level_str = level[0].upper()
    level_str += level[1:]
    full_level = f"{LOGGING_ENTRY_STR}.Level.{level_str}"
    return full_level


def expand_range(name):
    """
    expand_range(range_string) returns a list from strings with/without range specification at end

    expand a name such as:

        expand_range("name[1-5]")         -> ['name1', 'name2', 'name3', 'name4', 'name5']
        expand_range("unique")            -> ['unique']
        expand_range("other[0, 1 , 3-5]") -> ['other0', 'other1', 'other3', 'other4', 'other5']
        expand_range("chars[0, 1 , g-k]") ->
              ['chars0', 'chars1', 'charsg', 'charsh', 'charsi', 'charsj', 'charsk']

    """
    name_prefix = name
    name_range = name.split('[')
    list_names = []
    if len(name_range) > 1:
        name_prefix = name_range[0]
        for item_string in name_range[1].split(','):
            item = item_string.strip()
            if '-' not in item:
                list_names.append(name_prefix + item)
            else:
                values_range = item.split('-')
                value = values_range[0].strip()
                final_value = values_range[1].strip()
                while value <= final_value:
                    list_names.append(name_prefix + value)
                    value_int = ord(value) + 1
                    value = chr(value_int)
    else:
        list_names.append(name_prefix)
    return list_names


def lists_are_equal(list1, list2):
    """
    Compare two lists
    """
    if len(list1) != len(list2):
        return False
    return sorted(list1) == sorted(list2)


def is_event_mandatory_key(key):
    """
    Returns true if a such key belongs to MANDATORY_EVENT_KEYS list
    """
    if key.startswith(LOGGING_ENTRY_DOT_STR):
        key = key[len(LOGGING_ENTRY_DOT_STR):]
    for mandatory_key in MANDATORY_EVENT_KEYS:
        if isinstance(mandatory_key, list):
            if key in mandatory_key:
                return mandatory_key[0]
        elif key == mandatory_key:
            return mandatory_key
    return None


def is_event_optional_key(key):
    """
    Returns true if a such key belongs to OPTIONAL_EVENT_KEYS list
    """
    if key.startswith(LOGGING_ENTRY_DOT_STR):
        key = key[len(LOGGING_ENTRY_DOT_STR):]
    for optional_key in OPTIONAL_EVENT_KEYS:
        if isinstance(optional_key, list):
            if key in optional_key:
                return optional_key[0]
        elif key == optional_key:
            return optional_key
    return None


def __compare_event_data_and_redfish_data(key_list, mandatory_flag, injected_dict, redfish_dict):
    """
    Generic Private function to compare Event data between 'inject events' and
       'events collected' from redifsh
    The key_list can be either MANDATORY_EVENT_KEYS or OPTIONAL_EVENT_KEYS
    """
    ret = True
    try:
        for key in key_list:
            dict_key = key[0] if isinstance(key, list) else key
            if mandatory_flag is False:
                exist_both_keys = dict_key in injected_dict and dict_key in redfish_dict
                if not exist_both_keys:
                    continue
            elif dict_key == KEY_SEVERITY: # special Severity cheking
                sub_severity_key = redfish_dict[KEY_SEVERITY ].lower()
                if injected_dict[KEY_SEVERITY] not in SEVERITYDBUSTOREDFISH[sub_severity_key]:
                    ret = False
                    break
                continue  ## Severity field OK
            ## filds comparing
            if isinstance(redfish_dict[dict_key], list):
                injected_list_value = re.split(',\s*', injected_dict[dict_key])
                if lists_are_equal(redfish_dict[dict_key], injected_list_value):
                    continue
                ret = False
                break
            if  injected_dict[dict_key] != redfish_dict[dict_key]:
                ret = False
                break
    except Exception as error:
        raise error
    return ret


def compare_mandatory_event_fields(injected_dict, redfish_dict):
    """
    Compare mandatory Event Data Fields between 'inject events' and events collected  from redifsh
    """
    return __compare_event_data_and_redfish_data(MANDATORY_EVENT_KEYS, MANDATORY_FLAG,
                                               injected_dict, redfish_dict)


def compare_optional_event_fields(injected_dict, redfish_dict):
    """
    Compare optional Event Data Fields between 'inject events' and events collected  from redifsh
    """
    return __compare_event_data_and_redfish_data(OPTIONAL_EVENT_KEYS, OPTIONAL_FLAG,
                                               injected_dict, redfish_dict)

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
JSON_ADDITIONAL_FIELDS = ["resolution", "accessor", "event_trigger"]


# Keys used in dicts
KEY_MESSAGE_ID = "MessageId"
KEY_SEVERITY = 'Severity'

KEY_ACCESSOR = "Accessor"
KEY_ACCESSOR_TYPE      = KEY_ACCESSOR + ".Type"
KEY_ACCESSOR_OBJECT    = KEY_ACCESSOR + ".Object"
KEY_ACCESSOR_INTERFACE = KEY_ACCESSOR + ".Interface"
KEY_ACCESSOR_PROPERTY  = KEY_ACCESSOR + ".Property"
KEY_ACCESSOR_CHECK     = KEY_ACCESSOR + ".Check"
KEY_ACCESSOR_CHECK_BITMASK = KEY_ACCESSOR_CHECK + ".Bitmask"
KEY_ACCESSOR_CHECK_LOOKUP  = KEY_ACCESSOR_CHECK + ".Lookup"
KEY_ACCESSOR_CHECK_EQUAL   = KEY_ACCESSOR_CHECK + ".Equal"
KEY_ACCESSOR_CHECK_NOTEQUAL = KEY_ACCESSOR_CHECK + ".Not_equal"

KEY_ACCESSOR_DEP           = KEY_ACCESSOR + ".Dependency"
KEY_ACCESSOR_DEP_TYPE      = KEY_ACCESSOR_DEP + ".Type"
KEY_ACCESSOR_DEP_PROPERTY  = KEY_ACCESSOR_DEP + ".Property"

KEY_EVENT_TRIGGER = "Event_trigger"
KEY_EVENT_TRIGGER_TYPE      = KEY_EVENT_TRIGGER + ".Type"
KEY_EVENT_TRIGGER_OBJECT    = KEY_EVENT_TRIGGER + ".Object"
KEY_EVENT_TRIGGER_INTERFACE = KEY_EVENT_TRIGGER + ".Interface"
KEY_EVENT_TRIGGER_PROPERTY  = KEY_EVENT_TRIGGER + ".Property"
KEY_EVENT_TRIGGER_CHECK     = KEY_EVENT_TRIGGER + ".Check"
KEY_EVENT_TRIGGER_CHECK_BITMASK = KEY_EVENT_TRIGGER_CHECK + ".Bitmask"
KEY_EVENT_TRIGGER_CHECK_LOOKUP  = KEY_EVENT_TRIGGER_CHECK + ".Lookup"

## current device being handled
DEVICE_HW = ""

# common constants, it is used for both KEY_ACCESSOR AND KEY_EVENT_TRIGGER
ACCESSOR_TYPE_DBUS      = "DBUS"
ACCESSOR_TYPE_DBUS_CALL = "DBUS CALL"
ACCESSOR_TYPE_DEVICE_CORE_API = "DEVICECOREAPI"

LOGGING_ENTRY_STR       = "xyz.openbmc_project.Logging.Entry"
LOGGING_ENTRY_DOT_STR   = f"{LOGGING_ENTRY_STR}."


## this the index of the device defined in the input json file such as
##   "GPU": [ .. ]  -> GPU0, GPU1, ...  index= 0, 1, ...
CURRENT_JSON_DEVICE_INDEX = 0

# index in busctl_info
INDEX_DEVICE_NAME     = 0
INDEX_EVENT           = 1


# BUSCTL command index for Event Logging inserting -> Corresponding field position
BUSCTL_MSG_IDX            = 7   # "GPU0 OverT"
BUSCTL_SEVERITY_IDX       = 8   # "xyz.openbmc_project.Logging.Entry.Level.Critical"
BUSCTL_ADDITIONALDATA_IDX = 10  # where additinal data starts


# Keys used for comparing 'injected events' and 'redfish data'
#   if an element is a list: the first element is the key and the second is an alias
MANDATORY_EVENT_KEYS = [ KEY_SEVERITY, 'Resolution', [KEY_MESSAGE_ID, REDFISH_MESSAGE_ID] ]
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
    if level.lower() == "ok":
        full_level = f"{LOGGING_ENTRY_STR}.Level.Notice"
    else:
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
            # injected_dict can use the alias key in MANDATORY_EVENT_KEYS or OPTIONAL_EVENT_KEYS
            injected_key = key
            if isinstance(key, list):
                # injected_dict is using the alias key
                injected_key = key[1] if key[1] in injected_dict else key[0]
                key = key[0]
            exist_both_keys = injected_key in injected_dict and key in redfish_dict
            if exist_both_keys is False:
                if mandatory_flag is True:
                    print(f"\tError: Key '{key}' does not exist in redfish entry")
                    ret =  False  # both dicts must have the key value
                continue          # ok if the key information misses in one or both dicts
            # performs the comparing
            if key == KEY_SEVERITY: # special Severity cheking
                sub_severity_key = redfish_dict[key].lower()
                if injected_dict[injected_key] not in SEVERITYDBUSTOREDFISH[sub_severity_key]:
                    ret = False
                continue  ## Severity field OK
            ## filds comparing
            if isinstance(redfish_dict[key], list):
                injected_list_value = re.split(r',\s*', injected_dict[injected_key])
                if lists_are_equal(redfish_dict[key], injected_list_value):
                    continue
                print(f"\tError: the content of the lists of Key '{key}' does not match, redfish={redfish_dict[key]} injected={injected_list_value}")
                ret = False
            value_injected = injected_dict[injected_key]
            value_redfish = redfish_dict[key]
            ## compare strings without spaces, this works around some event creation problems
            if isinstance(value_injected, str):
                value_injected = ''.join(value_injected.split())
            if isinstance(value_redfish, str):
                value_redfish = ''.join(value_redfish.split())
            if  value_injected != value_redfish:
                print(f"\tError: the value of Key '{key}' does not match, redfish={value_redfish} injected={value_injected}")
                ret = False
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

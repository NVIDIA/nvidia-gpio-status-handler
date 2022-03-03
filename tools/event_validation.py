#!/usr/bin/python3

# --------------------------------------------------------------------------
# This script injects events using event_injector script and verify
# log creation by BMCWeb
#
# Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

from requests import get
from requests.auth import HTTPBasicAuth
import paramiko
from getpass import getpass
from shlex import split
from time import sleep
from termcolor import colored
import argparse
import json
import tempfile
import os

import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# Global variables used to access BMCWEB
BMCWEB_IP=""
BMCWEB_PORT=None
EVENT_LOG_URI="redfish/v1/Systems/system/LogServices/EventLog/Entries"

# Global variables used to access QEMU
QEMU_IP=""
QEMU_USER=""
QEMU_PASS=""
QEMU_PORT=None

# Global variables to access event injector script
EVENT_INJ_SCRIPT_PATH="/home/root/event_injector.bash"
EVENT_INJ_SCRIPT_ARGS=""
JSON_EVENTS_FILE=""

# BUSCTL command index -> Corresponding field
BUSCTL_MSG_IDX=7        # "GPU0 OverT"
BUSCTL_SEVERITY_IDX=8   # "xyz.openbmc_project.Logging.Entry.Level.Critical"
BUSCTL_MSGID_IDX=11     # "ResourceEvent.1.0.ResourceErrorsDetected"
BUSCTL_MSGARGS_IDX=13   # "["GPU0 PWR_GOOD status", "interrupt asserted"]"
BUSCTL_RES_IDX=15       # "Contact NVIDIA Support"

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

available_options = [
    {
        'args': [ '-p', '--passwd'],
        'kwargs': {
            'type': str,
            'default': None,
            'help': '''Specify Password to be used for QEMU and BMCWeb login.'''
        }
    },
    {
        'args': [ '--bmcweb-ip'],
        'kwargs': {
            'type': str,
            'default': '127.0.0.1',
            'help': '''Specify IP address to connect to BMCWeb.'''
        }
    },
    {
        'args': [ '--bmcweb-port'],
        'kwargs': {
            'type': int,
            'default': 2443,
            'help': '''Specify Port to connect to BMCWeb.'''
        }
    },
    {
        'args': [ '--qemu-ip'],
        'kwargs': {
            'type': str,
            'default': '127.0.0.1',
            'help': '''Specify IP address to connect to QEMU.'''
        }
    },
    {
        'args': [ '--qemu-port'],
        'kwargs': {
            'type': int,
            'default': 2222,
            'help': '''Specify Port to connect to QEMU.'''
        }
    },
    {
        'args': [ '-u', '--user' ],
        'kwargs': {
            'type': str,
            'default': 'root',
            'help': '''Specify Username to be used for QEMU and BMCWeb login.'''
        }
    },
    {
        'args': [ '-j', '--json' ],
        'kwargs': {
            'type': str,
            'default': '../examples/event_info.json',
            'help': '''A json file with events to be injected'''
        }
    },
    {
        'args': [ '--dry-run' ],
        'kwargs': {
            'action': 'store_true',
            'help': '''If this option is specified, no event will be injected.'''
        }
    },
]

def init_arg_parser():
    """
    Initialize argument parser
    """
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter,
            description='Event Injection Test Automation')
    for opt in available_options:
        parser.add_argument(*opt['args'], **opt['kwargs'])
    return parser

def parse_arguments():
    """
    Initialize argument parser.
    Set all the Global variables based on the arguments
    """
    parser = init_arg_parser()

    global QEMU_USER, QEMU_PASS, QEMU_IP, QEMU_PORT, BMCWEB_IP, BMCWEB_PORT, JSON_EVENTS_FILE, EVENT_INJ_SCRIPT_ARGS

    args, remaining = parser.parse_known_args()

    # QEMU Access info
    QEMU_USER= args.user
    QEMU_PASS=args.passwd if args.passwd else getpass(prompt='Password: ')
    QEMU_IP=args.qemu_ip
    QEMU_PORT=args.qemu_port

    # BMCWeb access info
    BMCWEB_IP=args.bmcweb_ip
    BMCWEB_PORT=args.bmcweb_port

    JSON_EVENTS_FILE=args.json

    # Check for dry run
    if not args.dry_run:
        EVENT_INJ_SCRIPT_ARGS = f"{EVENT_INJ_SCRIPT_ARGS} -r"

def lists_are_equal(list1, list2):
    if len(list1) != len(list2):
        return False

    return sorted(list1) == sorted(list2)


class Injector:
    """
    This class parses a Json Event file, then:
       1 Generates a bash script with busctl commands
    """

    def __init__(self, json_file):
        self._json_file  = json_file
        self._shell_file = ""
        self._shell_file_fd = 0
        self._busctl_cmd_counter = 0
        self._busctl_info = ["", "", ""]  # 3 items only, 0=device name, 1=severity
        self._INDEX_DEVICE_NAME = 0 # index for self._busctl_info
        self._INDEX_SEVERITY    = 1 # index for self._busctl_info
        self._INDEX_EVENT       = 2 # index for self._busctl_info
        self._key_REDFISH_MESSAGE_ID = "REDFISH_MESSAGE_ID"
        self._key_REDFISH_MESSAGE_ARGS = "REDFISH_MESSAGE_ARGS"
        self._additional_data = dict()
        self._black_list      = ["accessor"] # fields not parsed from json file


    def __del__(self):
        self.__remove_script_file()


    def script_file(self):
        return self._shell_file


    def __remove_script_file(self):
        if len(self._shell_file) > 0 and os.access(self._shell_file, os.W_OK):
            os.unlink(self._shell_file)


    def __create_script_file(self):
        fd, self._shell_file = tempfile.mkstemp()
        os.close(fd)
        self._shell_file_fd = open(self._shell_file, 'w')
        self._shell_file_fd.write("#!/bin/bash\n\n"
                                  "# This script is generated by event_validation.py, be aware that changes will be lost!\n\n"
                                  "DRY_RUN=1 # default do not run commands\n"
                                  "[ \"$1\" = \"-r\" -o \"$1\" = \"--run\" ] && DRY_RUN=0\n\n"
                                  "TEMPFILE=/tmp/$$.tmp\n"
                                  "echo 0 > $TEMPFILE\n\n"
                                  "run_cmd()\n"
                                  "{\n"
                                  "    cmd=\"$1\"\n"
                                  "    event=\"$2\"\n"
                                  "    echo $cmd\n"
                                  "    [ $DRY_RUN -eq 1 ] && return\n"
                                  "    eval \"$cmd\"\n"
                                  "    rc=$?\n"
                                  "    echo -n \"Injecting Device Event Log ($event) to D-Bus: \"\n"
                                  "    if [ $rc -ne 0 ]; then\n"
                                  "       echo \"Failed\"\n"
                                  "    else\n"
                                  "       echo \"Passed\"\n"
                                  "       # Fetch the value and increase it\n"
                                  "       COUNTER=$[$(cat $TEMPFILE) + 1]\n"
                                  "       # Store the new value\n"
                                  "       echo $COUNTER > $TEMPFILE\n"
                                  "    fi\n"
                                  "}\n"
                                 )


    def __close_script_file(self):
        self._shell_file_fd.write("\n\ninjections=$(cat $TEMPFILE)\n"
                                  "echo \"Successful Injections: $injections\"\n"
                                  "unlink $TEMPFILE\n\n")
        self._shell_file_fd.close()


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


    def __extends_field(self, field_str, value):
        first_char = field_str[0]
        field_str = first_char.upper() + field_str[1:]
        if isinstance(value, dict):
            subkeys = value.keys()
            for subfield in subkeys:
               subfield_str = subfield[0]
               subfield_str = subfield_str[0].upper() + subfield[1:]
               child_str = field_str + '.' + subfield_str
               self.__extends_field(child_str, value[subfield])
        else:
            key_field = "xyz.openbmc_project.Logging.Entry." + field_str
            if isinstance(value, list):
                value = ", ".join(value)
            if not isinstance(value, str):
                value = str(value)
            self._additional_data[key_field] = value


    def __parse_dict_data(self, device_name, data_device):
        fields_device = list(data_device.keys())
        self._busctl_info[self._INDEX_DEVICE_NAME]= device_name
        self._busctl_info[self._INDEX_SEVERITY] = "xyz.openbmc_project.Logging.Entry.Level.Notice"
        self._additional_data.clear()

        # REDFISH_MESSAGE_ARGS Not used so far, but it seems like necessary
        self._additional_data[self._key_REDFISH_MESSAGE_ARGS] = device_name + "_None, Fixme"

        ## this is the position to put the counter of custom field_and_value
        for field in fields_device:
            value = data_device[field]
            if field == "event":
                  self._busctl_info[self._INDEX_DEVICE_NAME]= device_name  + " " + value
                  self._busctl_info[self._INDEX_EVENT]= value
            elif field == "redfish":
                  self._additional_data[self._key_REDFISH_MESSAGE_ID] = value["message_id"] # redfish.message_id
            elif field == "severity":
                value_lower = value.lower();
                if value_lower == "warning":
                     self._busctl_info[self._INDEX_SEVERITY] = "xyz.openbmc_project.Logging.Entry.Level.Warning"
                elif value_lower == "critical":
                     self._busctl_info[self._INDEX_SEVERITY] = "xyz.openbmc_project.Logging.Entry.Level.Critical"
            elif field == "resolution":
                  self.__extends_field(field, value)

          # Other fields not used so far
          #  elif field.lower() not in self._black_list:
                 ## other fields will be added into self._additional_data
          #       self.__extends_field(field, value)


    def __generate_busctl_command_from_json_dict(self, device, data):
        self.__parse_dict_data(device, data)
        cmd  =  self.__create_bustcl_command()
        event_str = f'EVENT=\"{self._busctl_info[self._INDEX_EVENT]}\"\n'
        cmd_str   = f'CMD=\"{cmd}\"\n'
        run_str = 'run_cmd \"$CMD\" \"$EVENT\"'
        comments  = f"\n\n# https://127.0.0.1:2443/redfish/v1/Systems/system/LogServices/EventLog/Entries/{self._busctl_cmd_counter}\n"
        comments += f"# busctl introspect xyz.openbmc_project.Logging /xyz/openbmc_project/logging/entry/{self._busctl_cmd_counter}\n"
        self._shell_file_fd.write(comments)
        self._shell_file_fd.write(event_str)
        self._shell_file_fd.write(cmd_str)
        self._shell_file_fd.write(run_str)


    def __generate_device_busctl_command(self, device, data):
        if isinstance(data, dict):
            self.__generate_busctl_command_from_json_dict(device, data)
        else:
            for index in  range(len(data)):
                device_name = device+str(index)
                self.__generate_busctl_command_from_json_dict(device_name, data[index])


    def generate_script_from_json(self):
        """
        1. parses the Json Events file
        1. generates s shell file with busctl commands
        """
        fd = 0
        try:
            fd = open(self._json_file)
        except Exception as e:
            raise Exception(f"Json events input file not found:  {self._json_file} : {str(e)}")

        data = json.load(fd)
        key_list = list(data.keys())
        if len(key_list) > 0:
            self.__create_script_file()
        for device in key_list:
            data_dict = data[device]
            self.__generate_device_busctl_command(device, data_dict)
        if len(key_list) > 0:
            self.__close_script_file()


class InjectTest:
    """
    This class is used to:
        1.  Get all the logs already present using BMCWeb redfish interface, using API call.
        2.  Inject events into D-Bus using "event_injector" script.
        3.  Get all the logs generated after Step 2 and verify all the logs with corresponding events.
    """

    def __init__(self):
        """
        self.event_log_entries_api      -   API endpoint used to gather the logs
        self.initial_log_count          -   Number of logs before event injection
        self.total_events               -   Total number of events
        self.events_injected_count      -   Number of events injected
        self.events_injected            -   Information of all the events
        self.final_log_count            -   Number of logs after event injection
        self.log_cache                  -   Cache of Log Entry API
        """
        self.event_log_entries_api= f"https://{BMCWEB_IP}:{BMCWEB_PORT}/{EVENT_LOG_URI}"
        self.initial_log_count=0
        self.total_events=0
        self.events_injected_count=0
        self.events_injected=dict()

        self.final_log_count=0

        self.log_cache=None


    def current_log_count(self, cache=False):
        """
        Uses redfish API to get the Log entries.
        Uses key 'Member@odata.count' to get the current count.
        Cache the redfish API response if argument "cache" is set to "True"
        """
        try:
            # Call the API
            response = get(self.event_log_entries_api, verify=False,
                                    auth = HTTPBasicAuth(QEMU_USER, QEMU_PASS))
        except Exception as e:
            raise Exception(f"Exception occurred while making API({self.event_log_entries_api}) call: {str(e)}")

        try:
            # Convert the API response into JSON format
            response_json=response.json()
        except Exception as e:
            raise Exception(f"Exception occurred while converting response to json: {str(e)}\n Response: {response.text}")

        key_members = "Members"

        # Get the 'Members@odata.count' key to read the log count
        members = response_json.get(key_members, None)
        if members is None:
            raise Exception(f"Exception occurred while reading key: '{members}'")

        # Cache the API response if argument is set to True
        if cache:
            try:
                self.log_cache=response_json.copy()
            except Exception as e:
                raise Exception(f"Exception occurred while copying API response json: {str(e)}")

        log_count = 0

        for member in members:
            log_count = max(log_count, int(member['Id']))

        return log_count


    def collect_logs_before_injections(self):
        """
        Fetch current number of logs.
        """

        print("Fetching current logs....")

        try:
            self.initial_log_count = self.current_log_count()
        except Exception as e:
            raise e


    def inject_events(self):
        """
        *  Inject events using event_injector script over SSH
        *  Check stdout, stderr for any errors/exceptions
        *  Read all the busctl commands injected by the script and
            cache useful data in dictionaries
        """

        injector = Injector(JSON_EVENTS_FILE)
        print("...\n\nParsing Json file and generating event injector bash script....")
        injector.generate_script_from_json()

        current_log_number = self.initial_log_count

        try:
            # Create the SSH client
            ssh_cmd = paramiko.SSHClient()

            # Set missing host key policy
            ssh_cmd.set_missing_host_key_policy(paramiko.AutoAddPolicy())

            # Connect the SSH client to QEMU
            ssh_cmd.connect(QEMU_IP, username=QEMU_USER, port=QEMU_PORT, password=QEMU_PASS,timeout=5)
        except Exception as e:
            raise Exception(f"Exception occurred while connecting to {BMCWEB_IP} {QEMU_PORT}: {str(e)}")

        try:
            try:
                source_script = injector.script_file()
                print(f"...\n\nCopying {source_script} to {QEMU_USER}@{BMCWEB_IP}:{EVENT_INJ_SCRIPT_PATH} using sftp....")
                sftp = ssh_cmd.open_sftp()
                sftp.put(source_script, EVENT_INJ_SCRIPT_PATH)
                sftp.close()
            except Exception as e:
                raise Exception(f"Exception occurred while copying  {source_script} using sftp: {str(e)}")

            event_inject_cmd = f"/bin/bash {EVENT_INJ_SCRIPT_PATH} {EVENT_INJ_SCRIPT_ARGS}"

            print("...\n\nInjecting new events....")

            try:
                # Execute the command to invoke event_injector script
                stdin, stdout, stderr = ssh_cmd.exec_command(event_inject_cmd, get_pty=True)
            except Exception as e:
                raise Exception(f"Exception occurred while running command {event_inject_cmd} over ssh: {str(e)}")

            try:
                # Read stderr
                result_stderr = stderr.readline().strip()
            except Exception as e:
                raise Exception(f"Exception occurred while reading stderr for command {event_inject_cmd}: {str(e)}")

            # Check stderr output to check for error
            if result_stderr:
                raise Exception(f"Exception occurred while running command {event_inject_cmd} over ssh: {result_stderr}")


            try:
                # Read stdout
                result_stdout = stdout.readlines()
            except Exception as e:
                raise Exception(f"Exception occurred while reading stdout for command {event_inject_cmd}: {str(e)}")

            # Check stdout return code
            if stdout.channel.recv_exit_status():
                raise Exception(f"Exception occurred while running command {event_inject_cmd} over ssh:\n{''.join(result_stdout)}")

            # Print output from the event_injector script to the console
            print(''.join(result_stdout))

            # Iterate over all the commands returned by event_injector script
            for event in result_stdout:

                # Proceed only if it is a busctl command
                if "busctl" not in event:
                    continue

                # Split the busctl command and cache all the necessary fields/values
                try:
                    event_split = split(str(event))
                    current_log_number+=1
                    if current_log_number in self.events_injected:
                        print(f"Duplicate event: {current_log_number}")
                    else:
                        self.events_injected[current_log_number] = list()

                    # Create a temp dictionary to fill in the info
                    temp_dict = dict()
                    temp_dict["msg_id"] = event_split[BUSCTL_MSGID_IDX]
                    temp_dict["severity"] = event_split[BUSCTL_SEVERITY_IDX]
                    temp_dict["msg_args"] = [x.strip() for x in event_split[BUSCTL_MSGARGS_IDX].split(",")]
                    temp_dict["resolution"] = event_split[BUSCTL_RES_IDX]

                    """
                    self.events_injected looks like:

                    "GPU0 OverT": {
                        "msg_id" : "ResourceEvent.1.0.ResourceErrorsDetected,
                        "severity": "xyz.openbmc_project.Logging.Entry.Level.Critical",
                        "msg_args": "["GPU0_Temp", "Overheat"]",
                        "resolution": "Contact NVIDIA Support"
                    },
                    """
                    # Increment total number of events
                    self.total_events+=1
                    temp_temp_dict = temp_dict.copy()
                    self.events_injected[current_log_number] = temp_temp_dict
                except Exception as e:
                    print(f"Unable to get field from busctl command {event} {str(e)}")
                    pass

            try:
                self.events_injected_count = int((result_stdout[-1].strip().split(':')[-1].strip()))
            except Exception as e:
                raise Exception(f"Exception occurred while reading successful injections count: {str(e)}")

        except Exception as e:
            raise e
        finally:
            ssh_cmd.close()


    def collect_logs_after_injection_and_verify(self):
        """
        *  Gather all the logs generated after event injection
        *  Compare the logs information with the injected_events information cached in the inject_events method
        """
        print("...\nFetching new logs....")

        out = f""

        # If no event is injected, exit
        if self.events_injected_count == 0:
            raise Exception(f"No events injected, exiting!!")

        try:

            # Gather all the logs using BMCWeb interface
            self.final_log_count = self.current_log_count(cache=True)
        except Exception as e:
            raise e
        # Iterate over all the log entries
        for member in self.log_cache.get('Members', []):
            try:
                # Check if log entry number is greater than the initial logs count before injection
                if int(member['Id']) in self.events_injected:

                    log_id = int(member['Id'])

                    temp_dict = self.events_injected[log_id]

                    # Get event information for the log using the message
                    # Compare all the fields
                    if temp_dict["msg_id"] != member["MessageId"] or \
                                temp_dict["severity"] not in SEVERITYDBUSTOREDFISH[member["Severity"].lower()] or \
                                temp_dict["resolution"] != member["Resolution"] or \
                                not lists_are_equal(temp_dict["msg_args"], member["MessageArgs"]):
                                continue
                    # Remove the entry from the dictionary if all the fields are matched
                    # This confirms that a log is generated for an injected event
                    del self.events_injected[log_id]
            except Exception as e:
                out = f"{out}Exception occurred while matching keys for event {member['MessageArgs']}: {str(e)}\n"
                pass

        # If the dictionary still have some info,
        # that means we did not find logs for some events
        if len(self.events_injected) > 0:

            # Append the info in the output string and raise exception
            out = f"{out}\nUnable to verify log generation for following events:\n"
            for key in self.events_injected:
                if len(self.events_injected[key]) > 0:
                    out = f"{out}\t* {key}\n"

            raise Exception(f"Exception occurred: {out}")


    def print_summary(self):
        """
        *  Print the summary of the entire process
        Example:
        ====================
        SUMMARY:
            Total events: 6
            Events injected:  5
            Unverified events:  1
        """

        print("\n====================")
        print("SUMMARY:")
        print(f"\tTotal events: {self.total_events}")

        color = 'red' if self.events_injected_count < self.total_events else 'green'
        print(f"\tEvents injected: ", colored(f"{self.events_injected_count}", color))

        color = 'red' if len(self.events_injected) else 'green'
        print("\tUnverified events: ", colored(f"{len(self.events_injected)}", color))


def main():
    """
    *  Initialize argument parser
    *  Collect logs before injection
    *  Inject events
    *  Collect logs and verify after injection
    *  Print summary
    """

    # Initalize arg parser and add all the arguments
    parse_arguments()

    injectTest = InjectTest()

    try:
        injectTest.collect_logs_before_injections()

        injectTest.inject_events()

        sleep(2)

        injectTest.collect_logs_after_injection_and_verify()

    except Exception as e:
        print(colored(e, 'red'))
    finally:
        injectTest.print_summary()

if __name__ == "__main__":
    main()

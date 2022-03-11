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
import sys

from requests import get
from requests.auth import HTTPBasicAuth
import paramiko
from getpass import getpass
from shlex import split
from time import sleep
from termcolor import colored
import argparse
import os
import re

import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

## add modules sub directory from
modules_dir = os.path.dirname(__file__)
modules_dir_path = f"{modules_dir}/modules" if len(modules_dir) > 0 else "./modules"
sys.path.append(modules_dir_path)


from event_logging_script  import EventLogsInjectorScript
from event_logging_script  import LOGGING_ENTRY_DOT_STR
from event_accessor_script import EventAccessorInjectorScript

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
BUSCTL_ADDITIONALDATA_IDX=10

MANDATORY_EVENT_SEVERITY_KEY = 'Severity'
MANDATORY_EVENT_KEYS = [ MANDATORY_EVENT_SEVERITY_KEY, 'Resolution', ['MessageId', 'REDFISH_MESSAGE_ID'] ]
OPTIONAL_EVENT_KEYS  = [ ['MessageArgs', 'REDFISH_MESSAGE_ARGS'] ]
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

available_options = [
     {
        'args': [ '-m', '--mode'],
        'kwargs': {
            'type': int,
            'default': 1,
            'help': '''Mode to perform test strategy; 1=insert event logs; 2=change device status'''
        }
    },
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


def is_event_mandatory_key(key):
    if key.startswith(LOGGING_ENTRY_DOT_STR):
        key = key[len(LOGGING_ENTRY_DOT_STR):]
    for mandatory_key in MANDATORY_EVENT_KEYS:
        if isinstance(mandatory_key, list):
            if key == mandatory_key[0] or key == mandatory_key[1]:
                return mandatory_key[0]
        elif key == mandatory_key:
            return mandatory_key
    return None


def is_event_optional_key(key):
    if key.startswith(LOGGING_ENTRY_DOT_STR):
        key = key[len(LOGGING_ENTRY_DOT_STR):]
    for optional_key in OPTIONAL_EVENT_KEYS:
        if isinstance(optional_key, list):
            if key == optional_key[0] or key == optional_key[1]:
                return optional_key[0]
        elif key == optional_key:
            return optional_key
    return None


def compare_event_data_and_redfish_data(key_list, mandatory_flag, injected_dict, redfish_dict):
    ret = True
    try:
        for key in key_list:
            dict_key = key[0] if isinstance(key, list) else key
            if mandatory_flag is False:
                exists_key_event   = True if dict_key in injected_dict else False
                exists_key_redfish = True if dict_key in redfish_dict  else False
                if exists_key_event == False or exists_key_redfish == False:
                    continue
            elif dict_key == MANDATORY_EVENT_SEVERITY_KEY: # special Severity cheking
                sub_severity_key = redfish_dict[MANDATORY_EVENT_SEVERITY_KEY ].lower()
                if injected_dict[MANDATORY_EVENT_SEVERITY_KEY] not in SEVERITYDBUSTOREDFISH[sub_severity_key]:
                    ret = False
                    break
                else:
                    continue  ## Severity field OK
            ## filds comparing
            if isinstance(redfish_dict[dict_key], list):
                injected_list_value = re.split(',\s*', injected_dict[dict_key])
                if lists_are_equal(redfish_dict[dict_key], injected_list_value):
                        continue
                else:
                    ret = False
                    break
            if  injected_dict[dict_key] != redfish_dict[dict_key]:
                ret = False
                break
    except Exception as exec:
        raise exec
    return ret


def init_arg_parser():
    """
    Initialize argument parser
    """
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter,
            description='Event Injection Test Automation: mode=1 by \'inserting event logs\';  mode=2 by \'changing devices status (test for complete flow)\'')
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
        try: # Call the API
            response = get(self.event_log_entries_api, verify=False,
                                    auth = HTTPBasicAuth(QEMU_USER, QEMU_PASS))
        except Exception as e:
            raise Exception(f"Exception occurred while making API({self.event_log_entries_api}) call: {str(e)}")
        try:
            # Convert the API response into JSON format
            response_json=response.json()
        except Exception as e:
            raise Exception(f"Exception occurred while converting response to json: {str(e)}\n Response: {response.text}")
        # Get the 'Members@odata.count' key to read the log count
        key_members = "Members"
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


    def create_ssh_session(self):
        try:
            ssh_cmd = paramiko.SSHClient()
            # Set missing host key policy
            ssh_cmd.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            # Connect the SSH client to QEMU
            ssh_cmd.connect(QEMU_IP, username=QEMU_USER, port=QEMU_PORT, password=QEMU_PASS,timeout=5)
        except Exception as e:
            raise Exception(f"Exception occurred while connecting to {BMCWEB_IP} {QEMU_PORT}: {str(e)}")
        return ssh_cmd


    def sftp_script_to_emulator(self, ssh_client, source_script):
        try:
            print(f"...\n\nCopying {source_script} to {QEMU_USER}@{BMCWEB_IP}:{EVENT_INJ_SCRIPT_PATH} using sftp....")
            sftp = ssh_client.open_sftp()
            sftp.put(source_script, EVENT_INJ_SCRIPT_PATH)
            sftp.close()
        except Exception as e:
            raise Exception(f"Exception occurred while copying  {source_script} using sftp: {str(e)}")


    def execute_remote_script_and_get_stdout(self, ssh_client):
        event_inject_cmd = f"/bin/bash {EVENT_INJ_SCRIPT_PATH} {EVENT_INJ_SCRIPT_ARGS}"
        print("...\n\nInjecting new events....")
        try:
            # Execute the command to invoke event_injector script
            stdin, stdout, stderr = ssh_client.exec_command(event_inject_cmd, get_pty=True)
        except Exception as e:
            raise Exception(f"Exception occurred while running command {event_inject_cmd} over ssh: {str(e)}")
        # Read stderr
        try:
            result_stderr = stderr.readline().strip()
        except Exception as e:
            raise Exception(f"Exception occurred while reading stderr for command {event_inject_cmd}: {str(e)}")
        # Check stderr output to check for error
        if result_stderr:
            raise Exception(f"Exception occurred while running command {event_inject_cmd} over ssh: {result_stderr}")
        # Read stdout
        try:
            result_stdout = stdout.readlines()
        except Exception as e:
            raise Exception(f"Exception occurred while reading stdout for command {event_inject_cmd}: {str(e)}")
        # Check stdout return code
        if stdout.channel.recv_exit_status():
            raise Exception(f"Exception occurred while running command {event_inject_cmd} over ssh:\n{''.join(result_stdout)}")
        # Print output from the event_injector script to the console
        print(''.join(result_stdout))
        return result_stdout


    def parse_event_logging_output_get_events_injected(self, result_stdout):
        """
        1. reads stdout and inserts data in the dict self.events_injected
        2. self.events_injected keys are new 'events Ids' supposed as created
           these 'events Ids' are greater than the number of logs found before injection
        3. sets self.total_events          -> lines rom stdout containing a 'busctl' commands
        4. sets self.events_injected_count -> last stdout line such as 'Successful Injections: 23'

        self.events_injected looks like:

        "GPU0 OverT": {
            "msg_id" : "ResourceEvent.1.0.ResourceErrorsDetected,
            "severity": "xyz.openbmc_project.Logging.Entry.Level.Critical",
            "msg_args": "["GPU0_Temp", "Overheat"]",
            "resolution": "Contact NVIDIA Support"
        },
        """
        current_log_number = self.initial_log_count # number of logs found before injection
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
                temp_dict[MANDATORY_EVENT_SEVERITY_KEY]  = event_split[BUSCTL_SEVERITY_IDX]
                additional_data_idx = BUSCTL_ADDITIONALDATA_IDX
                while additional_data_idx < len(event_split):
                    key = is_event_mandatory_key(event_split[additional_data_idx])
                    if key is None:
                        key = is_event_optional_key(event_split[additional_data_idx])
                    if key is not None:
                        temp_dict[key] = event_split[additional_data_idx + 1]
                    additional_data_idx += 2
                # Increment total number of events
                self.total_events+=1
                temp_temp_dict = temp_dict.copy()
                self.events_injected[current_log_number] = temp_temp_dict
            except Exception as e:
                print(f"Unable to get field from busctl command {event} {str(e)}")
                pass
        # reads the last output line which should have a 'Successful Injections' counter
        try:
            self.events_injected_count = int((result_stdout[-1].strip().split(':')[-1].strip()))
        except Exception as e:
            raise Exception(f"Exception occurred while reading successful injections count: {str(e)}")


    def inject_events(self):
        """
        *  Inject events using event_injector script over SSH
        *  Check stdout, stderr for any errors/exceptions
        *  Read all the busctl commands injected by the script and
            cache useful data in dictionaries
        """
        event_logs_script = EventLogsInjectorScript(JSON_EVENTS_FILE)
        print("...\n\nParsing Json file and generating event EventLogsInjectorScript bash script....")
        event_logs_script.generate_script_from_json()

        try:
            # Create the SSH client
            ssh_cmd = self.create_ssh_session()
            # Set missing host key policy
            ssh_cmd.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            # Connect the SSH client to QEMU
            ssh_cmd.connect(QEMU_IP, username=QEMU_USER, port=QEMU_PORT, password=QEMU_PASS,timeout=5)
            # copy script
            self.sftp_script_to_emulator(ssh_cmd, event_logs_script.script_file())
            result_stdout = self.execute_remote_script_and_get_stdout(ssh_cmd)
            self.parse_event_logging_output_get_events_injected(result_stdout)
        except Exception as e:
            rais
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

        try: # Gather all the logs using BMCWeb interface
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
                    # Compare all the fields
                    mandatory_fields_match = compare_event_data_and_redfish_data(MANDATORY_EVENT_KEYS,
                                                                                 MANDATORY_FLAG,
                                                                                 temp_dict, member)
                    optional_fields_match = compare_event_data_and_redfish_data(OPTIONAL_EVENT_KEYS,
                                                                                OPTIONAL_FLAG,
                                                                                temp_dict, member)
                    if mandatory_fields_match == True and optional_fields_match == True:
                        del self.events_injected[log_id]
            except Exception as e:
                out = f"{out}Exception occurred while matching keys for event entry id {log_id}: {str(e)}\n"
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

        return color


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

    main_rc = os.EX_OK ;

    try:
        injectTest.collect_logs_before_injections()

        injectTest.inject_events()

        sleep(2)

        injectTest.collect_logs_after_injection_and_verify()

    except Exception as e:
        print(colored(e, 'red'))
        main_rc = -1
    finally:
        if main_rc == os.EX_OK and injectTest.print_summary() == 'green':
            main_rc = os.EX_OK

    return main_rc


if __name__ == "__main__":
    sys.exit(main())  # OS status will have 0 status if this test runs OK and passes

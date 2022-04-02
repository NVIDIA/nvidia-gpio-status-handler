#!/usr/bin/python3

"""
Event Inject tests
"""

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
import os

from getpass import getpass
from shlex import split
from time import sleep
import argparse
from requests import get
from requests.auth import HTTPBasicAuth
import paramiko

from termcolor import colored

import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

## add modules sub directory from
modules_dir = os.path.dirname(__file__)
modules_dir_path = f"{modules_dir}/modules" if len(modules_dir) > 0 else "./modules"
sys.path.append(modules_dir_path)
import com ## common constants and and functions
from event_logging_script  import EventLogsInjectorScript
from event_accessor_script import EventAccessorInjectorScript



# Global variables used to access BMCWEB
BMCWEB_IP=""
BMCWEB_PORT=None

# Global variables used to access QEMU
QEMU_IP=""
QEMU_USER=""
QEMU_PASS=""
QEMU_PORT=None

# Global variables to access event injector script
EVENT_INJ_SCRIPT_PATH="/home/root/event_injector.bash"
EVENT_INJ_SCRIPT_ARGS=""
JSON_EVENTS_FILE=""

TEST_MODE_EVENTS_LOGGING=1
TEST_MODE_CHANGE_DEVICE_STATUS=2
TEST_MODE=TEST_MODE_EVENTS_LOGGING
NOT_GENERATE_MESSAGE_ARGS = False

available_options = [
     {
        'args': [ '-m', '--mode'],
        'kwargs': {
            'type': int,
            'default': 1,
            'help': '''Test Mode to perform; 1=insert event logs; 2=change device status
                    (which should insert event logs)
                    '''
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


def avoid_unused_variable(variable):
    """
    It is used just to avoid pyling messages 'unused-variable', it does nothing
    """
    local_var = variable
    variable  = local_var


def init_arg_parser():
    """
    Initialize argument parser
    """
    msg  = "Event Injection Test Automation: mode=1 by \'inserting event logs\' "
    msg += ";  mode=2 by \'changing devices status (test for complete flow)\'"
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter,\
                                    description=msg)
    for opt in available_options:
        parser.add_argument(*opt['args'], **opt['kwargs'])
    return parser


def parse_arguments():
    """
    Initialize argument parser.
    Set all the Global variables based on the arguments
    Returns True if arguments are OK
    """
    parser = init_arg_parser()

    global QEMU_USER, QEMU_PASS, QEMU_IP, QEMU_PORT, BMCWEB_IP, BMCWEB_PORT, \
           JSON_EVENTS_FILE, TEST_MODE, EVENT_INJ_SCRIPT_ARGS

    args, remaining = parser.parse_known_args()
    avoid_unused_variable(remaining)

    TEST_MODE=args.mode

    if TEST_MODE < TEST_MODE_EVENTS_LOGGING or TEST_MODE > TEST_MODE_CHANGE_DEVICE_STATUS:
        print(f"Mode option -m|--mode {TEST_MODE} out of range, use -h to see valid mode options")
        return False

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

    return True


class InjectTest:
    """
    This class is used to:
        1.  Get all the logs already present using BMCWeb redfish interface, using API call.
        2.  Inject events into D-Bus using "event_injector" script.
        3.  Get all the logs generated after Step 2 and
              verify all the logs with corresponding events.
    """

    def __init__(self):
        """
        self.initial_log_count          -   Number of logs before event injection
        self.total_events               -   Total number of events
        self.events_injected_count      -   Number of events injected
        self.events_injected            -   Information of all the events
        self.final_log_count            -   Number of logs after event injection
        self.log_cache                  -   Cache of Log Entry API
        """
        self.initial_log_count=0
        self.total_events=0
        self.events_injected_count=0
        self.events_injected = {}
        self.final_log_count=0
        self._ssh_cmd = None
        self.log_cache=None


    def current_log_count(self, cache=False):
        """
        Uses redfish API to get the Log entries.
        Uses key 'Member@odata.count' to get the current count.
        Cache the redfish API response if argument "cache" is set to "True"
        """
        event_log_entries_api= f"https://{BMCWEB_IP}:{BMCWEB_PORT}/{com.EVENT_LOG_URI}"
        try: # Call the API
            response = get(event_log_entries_api, verify=False,
                                    auth = HTTPBasicAuth(QEMU_USER, QEMU_PASS))
        except Exception as error:
            msg = f"Exception occurred while making API({event_log_entries_api})"
            raise Exception(f"{msg} call: {str(error)}") from error
        try:
            # Convert the API response into JSON format
            response_json=response.json()
        except Exception as error:
            msg = f"Exception occurred while converting response to json: {str(error)}"
            raise Exception(f"{msg}\n Response: {response.text}") from error
        # Get the 'Members@odata.count' key to read the log count
        key_members = "Members"
        members = response_json.get(key_members, None)
        if members is None:
            raise Exception(f"Exception occurred while reading key: '{members}'")
        # Cache the API response if argument is set to True
        if cache:
            try:
                self.log_cache=response_json.copy()
            except Exception as error:
                msg = f"Exception occurred while copying API response json: {str(error)}"
                raise Exception(msg) from error
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
        except Exception as error:
            raise error


    def create_ssh_session(self):
        """
        Creates a ssh session
        """
        try:
            self._ssh_cmd = paramiko.SSHClient()
            # Set missing host key policy
            self._ssh_cmd.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            # Connect the SSH client to QEMU
            self._ssh_cmd.connect(QEMU_IP, username=QEMU_USER, port=QEMU_PORT, \
                              password=QEMU_PASS, timeout=5)
        except Exception as error:
            msg = f"Exception occurred while connecting to {BMCWEB_IP} {QEMU_PORT}"
            raise Exception(f"{msg}: {str(error)}") from error


    def sftp_script_to_emulator(self, source_script):
        """
        Just copies the script to emulator
        """
        try:
            msg = f"Copying {source_script} to {QEMU_USER}@{BMCWEB_IP}:{EVENT_INJ_SCRIPT_PATH}"
            print(f"...\n\n{msg} using sftp....")
            sftp =  self._ssh_cmd.open_sftp()
            sftp.put(source_script, EVENT_INJ_SCRIPT_PATH)
            sftp.close()
        except Exception as error:
            msg = f"Exception occurred while copying {source_script} using sftp"
            raise Exception(f"{msg}: {str(error)}") from error


    def execute_remote_script_and_get_stdout(self):
        """
        Runs the remote script and gets its output
        Returns its output
        """
        event_inject_cmd = f"/bin/bash {EVENT_INJ_SCRIPT_PATH} {EVENT_INJ_SCRIPT_ARGS}"
        result_stdout=""
        print("...\n\nInjecting new events....\n...")
        try:
            # Execute the command to invoke event_injector script
            channel = self._ssh_cmd.get_transport().open_session()
            channel.exec_command(event_inject_cmd)
            channel.set_combine_stderr(True)
            while True:
                if channel.exit_status_ready():
                    break
                out = channel.recv(256).decode("utf-8")
                print (out, end='')
                result_stdout += out
        except Exception as error:
            msg = f"Exception occurred while running command {event_inject_cmd}"
            raise Exception(f"{msg} over ssh: {str(error)}") from error

        # Check stdout return code
        if channel.recv_exit_status():
            msg  = f"Exception occurred while running command {event_inject_cmd} over ssh:"
            msg += f"\n{''.join(result_stdout)}"
            raise Exception(msg)
        return result_stdout.split('\n')

    def parse_event_logging_output_get_events_injected(self, result_stdout):
        """
        1. reads stdout and inserts data in the dict self.events_injected
        2. self.events_injected keys are new 'events Ids' supposed as created
           these 'events Ids' are greater than the number of logs found before injection
        3. sets self.total_events          -> lines rom stdout containing a 'busctl' commands

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
                    self.events_injected[current_log_number] = []
                # Create a temp dictionary to fill in the info
                temp_dict = {}
                temp_dict[com.KEY_SEVERITY]  = event_split[com.BUSCTL_SEVERITY_IDX]
                additional_data_idx = com.BUSCTL_ADDITIONALDATA_IDX
                while additional_data_idx < len(event_split):
                    key = com.is_event_mandatory_key(event_split[additional_data_idx])
                    if key is None:
                        key = com.is_event_optional_key(event_split[additional_data_idx])
                    if key is not None:
                        temp_dict[key] = event_split[additional_data_idx + 1]
                    additional_data_idx += 2
                # Increment total number of events
                self.total_events+=1
                temp_temp_dict = temp_dict.copy()
                self.events_injected[current_log_number] = temp_temp_dict
            except Exception as error:
                msg = f"Unable to get field from busctl command {event}"
                raise Exception(f"{msg}:  {str(error)}") from error


    def get_events_list_from_json_file(self, event_logs_script):
        """
        This function is similar to parse_event_logging_output_get_events_injected()
        Inserts data in the dict self.events_injected, to be compared to redfish data
        Sets self.total_events
        """
        current_log_number = self.initial_log_count # number of logs found before injection
        supposed_injected_events = event_logs_script.get_accessor_dbus_expected_events_list()
        for event_data in supposed_injected_events:
            current_log_number += 1
            self.events_injected[current_log_number] = event_data
            self.total_events  += 1

    def inject_events(self):
        """
        *  Inject events using event_injector script over SSH
        *  Check stdout, stderr for any errors/exceptions
        *  Read all the busctl commands injected by the script and
            cache useful data in dictionaries
        """
        event_logs_script = EventLogsInjectorScript(JSON_EVENTS_FILE, NOT_GENERATE_MESSAGE_ARGS) \
            if TEST_MODE == TEST_MODE_EVENTS_LOGGING else \
                EventAccessorInjectorScript(JSON_EVENTS_FILE)
        msg  = f"Parsing Json file {JSON_EVENTS_FILE} and generating event injector "
        msg += f"bash script for mode={TEST_MODE}...."
        print(f"...\n\n{msg}")
        event_logs_script.generate_script_from_json()

        try:
            # Create the SSH client
            self.create_ssh_session()
            self.sftp_script_to_emulator(event_logs_script.script_file())
            result_stdout = self.execute_remote_script_and_get_stdout()
            # reads the last output line which should have a 'Successful Injections: 23'
            try:
                index=-1
                while index > -4:  # try a couple on lines at bottom
                    if ':' in result_stdout[index]:
                        self.events_injected_count = int((result_stdout[index].strip().split(':')[-1].strip()))
                        break
                    index += -1
            except Exception as error:
                msg=f"Exception occurred while reading successful injections count: {str(error)}"
                raise Exception(msg) from error
            if TEST_MODE == TEST_MODE_EVENTS_LOGGING:
                self.parse_event_logging_output_get_events_injected(result_stdout)
            elif TEST_MODE == TEST_MODE_CHANGE_DEVICE_STATUS:
                self.get_events_list_from_json_file(event_logs_script)
        except Exception as error:
            raise error
        finally:
            self._ssh_cmd.close()


    def collect_logs_after_injection_and_verify(self):
        """
        *  Gather all the logs generated after event injection
        *  Compare the logs information with the injected_events information
             cached in the inject_events() method
        """
        print("Fetching new logs....\n...")

        # If no event is injected, exit
        if self.events_injected_count == 0:
            raise Exception("No events injected, exiting!!")

        try: # Gather all the logs using BMCWeb interface
            self.final_log_count = self.current_log_count(cache=True)
        except Exception as error:
            raise error

        out = ""
        # Iterate over all the log entries
        for member in self.log_cache.get('Members', []):
            try:
                # Check if log entry number is greater than the initial logs count before injection
                if int(member['Id']) in self.events_injected:
                    log_id = int(member['Id'])
                    temp_dict = self.events_injected[log_id]
                    # Compare all the fields
                    mandatory_fields_match = com.compare_mandatory_event_fields(temp_dict, member)
                    optional_fields_match = com.compare_optional_event_fields(temp_dict, member)
                    if mandatory_fields_match is True and optional_fields_match is True:
                        del self.events_injected[log_id]
            except Exception as error:
                message = "Exception occurred while matching keys for event id"
                out = f"{message} :{log_id}: {str(error)}\n"
                raise error

        # If the dictionary still have some info,
        # that means we did not find logs for some events
        if len(self.events_injected) > 0:
            # Append the info in the output string and raise exception
            out = f"{out}\nUnable to verify log generation for following events:\n"
            event_key_list = list(self.events_injected)
            for key in event_key_list:
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
        print("\tEvents injected: ", colored(f"{self.events_injected_count}", color))

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
    arguments_ok = parse_arguments()

    main_rc = os.EX_OK if arguments_ok is True else -1

    try:
        if main_rc == os.EX_OK:
            inject_test = InjectTest()
            inject_test.collect_logs_before_injections()
            inject_test.inject_events()
            sleep(2)
            inject_test.collect_logs_after_injection_and_verify()
    except Exception as error:
        print(colored(error, 'red'))
        main_rc = -1
        raise error
    finally:
        if main_rc == os.EX_OK and inject_test.print_summary() == 'green':
            main_rc = os.EX_OK

    return main_rc


if __name__ == "__main__":
    sys.exit(main())  # OS status will have 0 status if this test runs OK and passes


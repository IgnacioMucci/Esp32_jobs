#!/usr/bin/env python3
"""
PoMA/TCP Debug Client
Allows the user to connect to a PoMA server and execute timed commands defined in *.tpoma configuration files.
"""

import time
import argparse
import sys
from pathlib import Path
from typing import List

from modules.logger.logger import Logger
from modules.poma.poma_debug_client import PomaDebugClient
from modules.poma.timed_command import TimedCommand


def _parse_args():
    parser = argparse.ArgumentParser(
        description="""
    TCP client for debugging with timed PoMA commands.

    This routine connects to the specified PoMA server, executes a predefined set of timed commands, and then
    terminates the connection. The commands definition is obtained from a .tpoma file.
    """,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
    PoMA command definition file (.tpoma) format:
      timestamp|command

    Where timestamp is the time in seconds since the start of execution, and command is the PoMA command to send.

    Example:
      # This is a comment.
      0.0|*
      1.5|?g_var
      3.0|=g_var 100

    Lines that begin with # are comments and are ignored.
    """
    )

    parser.add_argument(
        '-i', '--ip',
        default='127.0.0.1',
        help='PoMA Server IP address (default: 127.0.0.1)'
    )

    parser.add_argument(
        '-p', '--port',
        type=int,
        default=3333,
        help='PoMA Server port (default: 3333)'
    )

    parser.add_argument(
        '-f', '--file',
        type=Path,
        required=False,
        help='PoMA command definition file (.tpoma)'
    )

    parser.add_argument(
        '-y', '--assume-yes',
        type=bool,
        required=False,
        default=False,
        help='Bypass confirmation prompts by automatically responding yes when requested'
    )

    return parser.parse_args()


def _load_commands(filepath: Path, logger: Logger) -> List[TimedCommand]:
    """
    Parses a commands definition file (.tpoma) and loads commands into a list of
    sorted TimedCommand objects. If the file is not passed, a default one is used.
    """
    commands = []
    if filepath is None:
        filepath = Path('./tests/default.tpoma')
        logger.info(f"No file specified. Using default definition.")

    logger.info(f"Loading commands from file '{filepath}'.")
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()

                # Ignore empty lines and comments.
                if not line or line.startswith('#'):
                    continue

                # Parse "timestamp|command".
                if '|' not in line:
                    logger.warning(f"Line {line_num} ignored (invalid format): '{line}'.")
                    continue

                parts = line.split('|', 1)
                try:
                    timestamp = float(parts[0])
                    command = parts[1].strip() + '\n'
                    commands.append(TimedCommand(timestamp, command))
                except ValueError:
                    logger.warning(f"Line {line_num} ignored (invalid timestamp): '{line}'.")
                    continue
    except FileNotFoundError:
        logger.error(f"File not found: '{filepath}'.")
    except Exception as e:
        logger.error(f"Error reading file: '{e}'.")

    # Sort commands by timestamp.
    commands.sort(key=lambda m: m.timestamp)

    logger.info(f"Loaded {len(commands)} commands from '{filepath}'.")
    return commands


def run_debug_session(client: PomaDebugClient, commands: List[TimedCommand], logger: Logger, assume_yes: bool = False):
    """Runs a debugging session by sending PoMA timed commands."""
    if not commands:
        logger.error("There are no commands to send.")
        return

    logger.info(f"Starting a debugging session with {len(commands)} commands.")

    start_time = time.time()
    for i, cmd in enumerate(commands, 1):
        # Check if the connection is still active.
        if not client.is_connected():
            logger.error("Connection lost. Stopping session.")
            break

        logger.blank_line()

        # Wait until the command timestamp.
        elapsed = time.time() - start_time
        wait_time = cmd.timestamp - elapsed
        if wait_time > 0:
            logger.waiting(f"Waiting {wait_time:.2f}s to send the next command (ts={cmd.timestamp:.2f}s)...")
            time.sleep(wait_time)

        # Show progress.
        elapsed = time.time() - start_time
        logger.section(f"command {i}/{len(commands)} (t={elapsed:.2f}s)")

        # Send command.
        send_time = time.time()
        if not client.send_command(cmd.command):
            if not assume_yes:
                logger.warning("Error sending command. ¿Continue? ([Y]/n)")
                resp = input().strip().lower()
                if resp not in ('', 'y'):
                    break
            else:
                logger.warning("Error sending command. Continuing automatically (assuming yes).")
            continue

        # Wait response.
        client.receive_response()
        received_time = time.time() - send_time
        logger.waiting(f"Response received after {received_time:.2f}s")

    total_time = time.time() - start_time
    logger.separator()
    logger.info(f"Session completed. Total time: {total_time:.2f}s.")


def main():
    args = _parse_args()
    logger = Logger()
    logger.separator()
    logger.section("PoMA Debug Client")
    logger.separator()
    logger.debug("Starting PoMA Debug Client with params:")
    logger.debug(f"IP:   {args.ip}")
    logger.debug(f"Port: {args.port}")
    if args.file:
        logger.debug(f"File: {args.file}")

    # Load commands.
    logger.blank_line()
    logger.section("Session Commands:")
    commands = _load_commands(args.file, logger)
    if not commands:
        logger.error("No commands loaded. Exiting.")
        sys.exit(1)
    else:
        logger.debug(f"{'Timestamp'.rjust(10)} : Command")
        logger.debug("----------------------------------------")
        for cmd in commands:
            logger.debug(f"{cmd.timestamp.__str__().rjust(10)} : {logger.unhide_especial_chars(cmd.command)}")

    # Create and connect client.
    logger.blank_line()
    logger.section("PoMA/TCP Connection:")
    client = PomaDebugClient(args.ip, args.port, logger)
    if not client.connect():
        logger.error("TCP connection could not be established. Exiting.")
        sys.exit(2)

    logger.blank_line()
    logger.separator()
    logger.section("Debug Session:")
    logger.separator()
    try:
        # PoMA Debugging/Testing session.
        run_debug_session(client, commands, logger, args.assume_yes)
    except KeyboardInterrupt:
        logger.error("Program interrupted by user.")
    except Exception as e:
        logger.error(f"Error during session: '{e}'.")
    finally:
        client.close()


if __name__ == '__main__':
    main()

# PoMA Test Client

TCP debug client for the PoMA protocol. Allows the user to connect to a PoMA server and execute timed commands defined
in `.tpoma` configuration files.

## Main Features

- Configurable TCP connection (IP and port).
- Execution of timed PoMA commands loaded from plain text files (`.tpoma`).
- Logging of outgoing and incoming messages showing any special characters (`\r`, `\n`, `\0`).

## Requirements

- Python v3.6 or higher.

## Usage

Basic Execution (default values):

```bash
python3 poma_test.py
```

or

```bash
./poma_test.py
```

Specifying the command file (`.tpoma`):

```bash
./poma_test.py -f example.tpoma
```

Specifying the PoMA server (IP/Port):

```bash
./poma_test.py -i 127.0.0.1 -p 3333
```

### Options

- `-h`/`--help`: Displays program help.
- `-i <ip>`/`--ip <ip>`: Specifies the PoMA server IP address. Default `127.0.0.1`.
- `-p <port>`/`--port <port>`: Specifies the PoMA server port. Default is `3333`.
- `-f <file>`/`--file <file>`: Specifies the `.tpoma` configuration file to use. If not specified, the file located at
  `./tests/default.tpoma` is used.
- `-y`/`--assume-yes`: Executes commands without confirmation to continue in case of error.

## .tpoma Files

These are plain text files where each line must adhere to the following structure:

- Begin with `#`: These lines are ignored and serve only to indicate comments.
- Format `TIMESTAMP|COMMAND`. These lines represent a timed command.
    - `TIMESTAMP` is the time that must elapse before the command can be executed, measured in seconds, expressed in
      decimals. Examples: `0.5`, `1.2`.
    - `COMMAND` is the PoMA command to be executed on the PoMA server. The entire rest of the line is taken as the
      command, and a newline character (`\n`) and a null character (`\0`) are added at the end. Example: `1.0|*`
      produces the TCP message `*\n\0`.

Commands can be in any order in the file. Before being executed, commands are ordered by ascending `TIMESTAMP` and
executed in that order.

Empty lines are ignored.

### Example

```
# This is a comment and is ignored.
# The following lines are timed commands:
0.0|*
1.5|?g_var
3.0|=g_var 100
6.0|?g_var
# Any other line format is invalid and is ignored. For example:
*|1.0
=g_var 100
+2.5|?g_var
```

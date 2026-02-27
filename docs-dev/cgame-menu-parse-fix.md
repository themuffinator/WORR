# UI Script Parsing Fix in Cgame

## Problem
UI menu scripts are tokenized by the engine, but option parsing in cgame uses a
separate `cmd_optind/cmd_optarg/cmd_optopt`. Because these locals were never
reset per line, option parsing drifted across menu script lines, producing
many "Usage:" errors and empty menu entries.

## Fix
Reset the cgame-local option parsing state in `Cmd_TokenizeString` before
forwarding to the engine tokenizer, matching the engine’s behavior.

## Result
Menu script commands parse with the correct arguments and menu entries are
created as expected.

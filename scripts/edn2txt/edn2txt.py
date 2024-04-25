#!/usr/bin/env python3

# Converts structured data from EDN format(elle history) into text format.

import sys
import json
import edn_format

# Check if command line argument is specified (it is mandatory).
if len(sys.argv) < 2:
  print("Usage:")
  print("  edn2txt.py input_file.edn")
  print("Example:")
  print("  edn2txt.py report.edn")
  sys.exit(1)

# First command line argument should contain name of input EDN file.
filename = sys.argv[1]
txn_id_cnt = 0
event_records = []


# Taken from https://github.com/swaroopch/edn_format/issues/76#issuecomment-749618312
def edn_to_map(x):
  if isinstance(x, edn_format.ImmutableDict):
    return {edn_to_map(k): edn_to_map(v) for k, v in x.items()}
  elif isinstance(x, edn_format.ImmutableList):
    return [edn_to_map(v) for v in x]
  elif isinstance(x, edn_format.Keyword):
    return x.name
  else:
    return x
        

def dump_txn(txn):
  global event_records
  global txn_id_cnt
  
  if txn['type'] != 'ok':
    return;
  session = txn['process'] + 1 # session_id 0 is reserved for the init session
  txn_id_cnt += 1
  events = txn['value']
  for evt in events:
    if evt[0] == 'append':
      key, value = evt[1:]
      record = f'{session} {txn_id_cnt} A {key} {value}'
    elif evt[0] == 'r':
      key, values = evt[1:]
      record = f'{session} {txn_id_cnt} R {key} '
      if values is None:
        values = [] # shift None to empty list
      record += str(len(values))
      for value in values:
        record += f' {value}'
    else:
      assert False
    event_records.append(record)

# Try to open the EDN file specified.
with open(filename, "r") as edn_input:
  # open the EDN file and parse it
  lines = edn_input.readlines()
  for line in lines:
    payload = edn_format.loads(line)
    # print(payload)
    dump_txn(edn_to_map(payload))
  print(len(event_records))
  for record in event_records:
    print(record)
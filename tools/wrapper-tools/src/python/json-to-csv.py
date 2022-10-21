#!/usr/bin/python

import pandas as pd
import sys

import common

def main():
    args = common.csvFileProc("Convert json file to csv format")
    with common.openInput(args.file_in, encoding='utf-8') as inputfile:
        df = pd.read_json(inputfile)    
    with common.openOutput(args.file_out, encoding='utf-8') as outputfile:
        df.to_csv(outputfile,
                  encoding = args.encoding,
                  quotechar = args.quotechar,
                  columns = args.columns,
                  sep = args.sep,
                  header = not args.no_header,
                  index = False)    
    return 0

if __name__ == "__main__":
    main()

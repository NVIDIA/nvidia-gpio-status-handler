#!/usr/bin/python

import pandas as pd
import sys

import argparse

def readArgs():
    parser = argparse.ArgumentParser(
        description = ("""Convert json file to csv format"""))
    defaultEncoding = "utf-8"
    parser.add_argument(
        "--encoding",
        type = str,
        action = "store",
        default = defaultEncoding,
        help = (f"Encoding of the csv file" +
                f"Default: '{defaultEncoding}'"))
    defaultQuotechar = '"'
    parser.add_argument(
        "--quotechar",
        type = str,
        action = "store",
        default = defaultQuotechar,
        help = (f"The character to quote fields in csv file" +
                f"Default: '{defaultQuotechar}'"))
    defaultSep = ","
    parser.add_argument(
        "--sep",
        type = str,
        action = "store",
        default = defaultSep,
        help = (f"Separator of columns for csv file. " +
                f"Default: '{defaultSep}'"))
    defaultColumns = None
    parser.add_argument(
        "--columns",
        type = str,
        action = "store",
        nargs = "*",
        default = defaultColumns,
        help = (f"List of columns to reduce the output to. " +
                f" Default: all"))
    parser.add_argument(
        "in_json",
        type = str,
        action = "store",
        nargs = "?",
        default = "-",
        help = (f"File path to read, or '-' for stdin. " +
                f"Default: '-'"))
    parser.add_argument(
        "out_csv",
        type = str,
        action = "store",
        nargs = "?",
        default = "-",
        help = (f"File path to write, or '-' for stdout. " +
                f"Default: '-'"))
    parser.add_argument(
        "--no_header",
        action = "store_true",
        help = (f"Whether to omit the header"))
    return parser.parse_args()

def openInput(filename, **rest):
    return (sys.stdin if filename == "-"
            else open(filename, "r", **rest))

def openOutput(filename, **rest):
    return (sys.stdout if filename == "-"
            else open(filename, "w", **rest))

def main():
    args = readArgs()
    with openInput(args.in_json, encoding='utf-8') as inputfile:
        df = pd.read_json(inputfile)    
    with openOutput(args.out_csv, encoding='utf-8') as outputfile:
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

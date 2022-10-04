#!/usr/bin/env python

import pandas as pd

import argparse

def readArgs():
    parser = argparse.ArgumentParser(
        description = ("Convert the table of CMDLINE testpoints into " +
                       "mockup definition template table"))
    parser.add_argument(
        "testpoints_cmdline_tsv",
        type = str,
        action = "store",
        help = (""))
    parser.add_argument(
        "output_tsv",
        type = str,
        action = "store",
        help = (f""))
    return parser.parse_args()

def main():
    args = readArgs()
    cmdlineTestpDf = pd.read_csv(
        args.testpoints_cmdline_tsv,
        sep = '	',
        quotechar = "'")
    n = cmdlineTestpDf.shape[0]
    filledParams = pd.DataFrame(data = {
        "time_ms": pd.Series(data=[0] * n),
        "output": cmdlineTestpDf["expected_value"],
        "return_code": pd.Series(data=[0] * n)
    })
    result = cmdlineTestpDf.join(filledParams)
    result.to_csv(args.output_tsv,
                  encoding = 'utf-8',
                  index = True,
                  sep = '	',
                  quotechar = "'")
    return 0

if __name__ == "__main__":
    main()

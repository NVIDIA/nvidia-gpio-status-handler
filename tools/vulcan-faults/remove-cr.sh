#!/bin/bash
I=$'\r'

# Google sheet encodes newlines inside a cell as a 'carriage
# return' ('\r') character. This complicates the csv file
# handling.

# Convert 'carriage return' characters into dollar signs '$' (but
# not those at the end of line in a '\r\n' sequence). This can
# remove the multi-line cells mess while keeping information
# about the newlines (not ideal though)

sed -e "s/${I}\(.\)/\\$\1/g" < "$1"

#!/bin/bash

cat my_file.bin > /dev/liana

hexdump my_file.bin > /tmp/output1

echo "Comparing /tmp/output and /tmp/output1 . . . "
diff /tmp/output /tmp/output1

#!/bin/bash
echo "=== Compiling Milestone 4 ==="
make clean
make milestone4

echo -e "\n=== Running Test with imput.txt ==="
./sim imput.txt

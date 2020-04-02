#!/bin/bash
git clean -xdf
cd CodeReverse2 && git clean -xdf && cd ..
cd payload/minhook && git clean -xdf && cd ..

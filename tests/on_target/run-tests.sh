#!/bin/bash

TOP_DIR="tests/on_target"

pip install -r $TOP_DIR/requirements.txt --break-system-packages

pytest -v -s $TOP_DIR/tests/test.py

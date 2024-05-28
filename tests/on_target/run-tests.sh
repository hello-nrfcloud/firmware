#!/bin/bash

pip install -r tests/on_target/requirements.txt --break-system-packages

pytest -v -s tests/on_target/tests/test.py

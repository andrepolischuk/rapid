#!/usr/bin/env bash

gcc src/*.c -o ./app -L /opt/homebrew/lib/ -l cjson && ./app $1

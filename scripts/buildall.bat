@echo off

pio run -t clean && pio run -t upload && pio run -t uploadfs

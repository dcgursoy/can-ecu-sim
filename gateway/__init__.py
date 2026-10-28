"""Fault-injection CAN gateway: forwards frames between two SocketCAN interfaces
and applies configurable faults on the way (gateway.faults holds the fault
engine, gateway.gateway the relay process and its command line)."""
